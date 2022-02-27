#include "thread"
#include "iostream"
#include "unistd.h"
#include <cstdlib>
#include "queue"
#include "mutex"
#include "condition_variable"
using namespace std;
struct DataA {
    int a;
};
struct DataB{
    double data[5];
};
const int MAX_DATA_A_CACHE_LENGTH = 128;
const int MAX_DATA_B_CACHE_LENGTH = 128;
queue<DataA> data_a_buffer;
queue<DataB> data_b_buffer;
mutex mutex_a;
mutex mutex_b;
condition_variable cond_producer_a;
condition_variable cond_producer_b;
condition_variable cond_consumer_b;

double sum_b_all = 0;
long long add_count_b = 0;
long long add_count_a = 0;
// 生产者A，整数发生器
[[noreturn]] void produceA(){
    while (true){
        unique_lock<mutex> lock_producer_a(mutex_a);
        // 队列已满，释放锁，等待唤醒
        while(data_a_buffer.size() > MAX_DATA_A_CACHE_LENGTH){
            cond_producer_a.wait(lock_producer_a);
        }
        // 队列有空，可以生产，消费者不需要等待A，不通知
        DataA tempA = {static_cast<int>(random() % 100 + 1)};
        data_a_buffer.push(tempA);
    }
}

[[noreturn]] void produceB(){
    while(true){
        unique_lock<mutex> lock_producer_b(mutex_b);
        // 队列已满，释放锁，等待唤醒
        while(data_b_buffer.size() > MAX_DATA_B_CACHE_LENGTH){
            cond_producer_b.wait(lock_producer_b);
        }
        // 队列有空，可以生产，通知等待的C
        DataB tempB{};
        for(double & i : tempB.data){
            i = static_cast<int>(random() % 100) / 100.0;
        }
        data_b_buffer.push(tempB);
        cond_consumer_b.notify_one();
    }
}

[[noreturn]] void processDataC(){
    int parity;
    while(data_a_buffer.empty()){}
    unique_lock<mutex> locker_consumer_a(mutex_a);
    parity = data_a_buffer.front().a & 1;
    data_a_buffer.pop();
    locker_consumer_a.unlock();
    while(true){
        // 取出B
        unique_lock<mutex> locker_consumer_b(mutex_b);
        while(data_b_buffer.empty()){
            cond_consumer_b.wait(locker_consumer_b);
        }
        DataB tempB = data_b_buffer.front();
        data_b_buffer.pop();
        cond_producer_b.notify_one();
        locker_consumer_b.unlock();
        double sum_b = 0;
        for(auto num : tempB.data){
            sum_b += num;
        }

        // 取出A
        locker_consumer_a.lock();
        if (!data_a_buffer.empty()){
            parity = data_a_buffer.front().a & 1;
            add_count_a ++;
            data_a_buffer.pop();
            cond_producer_a.notify_one();
        }
        locker_consumer_a.unlock();
        // 计算
        sum_b_all += (parity ? sum_b : -sum_b);
        add_count_b++;
    }
}

[[noreturn]]void printNum(){
    int time = 0;
    while (true){
        cout << "时间：" << time << "     收到B数据个数：" << add_count_b << "     收到A数据个数："<< add_count_a << "     当前计算结果：" << sum_b_all << endl;        fflush(stdout);
        ++time;
        sleep(1);
    }
}

int main(){
    // 启动打印线程
    thread D(printNum);
    D.detach();
    // 启动数据处理线程
    thread producer_data_a(produceA);
    thread producer_data_b(produceB);
    thread consumer_c(processDataC);
    // 等待线程
    producer_data_a.join();
    consumer_c.join();
    producer_data_b.join();
}