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
struct DataC{
    bool is_type_b; // 0代表a, 1代表b
    int a;
    double data[5];
};
const int MAX_DATA_CACHE_LENGTH = 256;
queue<DataC> data_buffer;
mutex m;
condition_variable cond_producer;
condition_variable cond_consumer;

double sum_b_all = 0;
long long add_count_b = 0;
long long add_count_a = 0;

[[noreturn]] void produceA(){
    while (true){
        unique_lock<mutex> lock_producer_a(m);
        // 队列已满，释放锁，等待唤醒
        while(data_buffer.size() > MAX_DATA_CACHE_LENGTH){
            cond_producer.wait(lock_producer_a);
        }
        // 队列有空，可以生产，通知消费者
        DataC tempC;
        tempC.is_type_b = false;
        tempC.a = static_cast<int>(random() % 100 + 1);
        data_buffer.push(tempC);
        cond_consumer.notify_one();
    }
}

[[noreturn]] void produceB(){
    while(true){
        unique_lock<mutex> lock_producer_b(m);
        // 队列已满，释放锁，等待唤醒
        while(data_buffer.size() > MAX_DATA_CACHE_LENGTH){
            cond_producer.wait(lock_producer_b);
        }
        // 队列有空，可以生产，通知等待的C
        DataC tempC{};
        tempC.is_type_b = true;
        for(double & i : tempC.data){
            i = static_cast<int>(random() % 100) / 100.0;
        }
        data_buffer.push(tempC);
        cond_consumer.notify_one();
    }
}

[[noreturn]] void processDataC(){
    int parity = 1;
    while(true){
        // 取出
        unique_lock<mutex> locker_consumer(m);
        while(data_buffer.empty()){
            cond_consumer.wait(locker_consumer);
        }
        DataC tempC = data_buffer.front();
        data_buffer.pop();
        locker_consumer.unlock();
        cond_producer.notify_all();
        // 是类型B
        if(tempC.is_type_b){
            double sum_b = 0;
            for(auto num : tempC.data){
                sum_b += num;
            }
            // 计算
            sum_b_all += (parity ? sum_b : -sum_b);
            add_count_b++;
        }
        // 是类型A
        else{
            parity = tempC.a & 1;
            add_count_a ++;
        }
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
