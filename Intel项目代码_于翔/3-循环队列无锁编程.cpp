#include "thread"
#include "iostream"
#include "unistd.h"
#include <cstdlib>
#include "mutex"
#include "condition_variable"
#include "queue"

using namespace std;
struct DataA {
    int a;
};
struct DataB{
    double data[5];
};

const int QueueSize = 256;
template <typename T> class CirQueue
{
public:
    typedef T value_type;
    CirQueue(){
        front = rear = 0;
    }
    ~CirQueue(){}
    T DeQueue(){
        front = (front + 1) % QueueSize;
        return data[front];
    }
    bool Empty() {
        return front == rear;
    }
    bool Full(){
        return (rear + 1) % QueueSize == front;
    }
    void EnQueue(T x)
    {
        if((rear + 1) % QueueSize == front)
            return;
        else
        {
            rear = (rear + 1) % QueueSize;
            data[rear] = x;
        }
    }
private:
    T data[QueueSize];
    int front, rear;
};
CirQueue<DataA> data_a_buffer;
CirQueue<DataB> data_b_buffer;

double sum_b_all = 0;
long long add_count_b = 0;
long long add_count_a = 0;

[[noreturn]] void produceA(){
    while (true){
        while(data_a_buffer.Full()){
            usleep(1);
        }
        DataA tempA = {static_cast<int>(random() % 100 + 1)};
        data_a_buffer.EnQueue(tempA);
    }
}

[[noreturn]] void produceB(){
    while(true){
        while(data_b_buffer.Full()){
            usleep(1);
        }
        DataB tempB{};
        for(double & i : tempB.data){
            i = static_cast<int>(random() % 100) / 100.0;
        }
        data_b_buffer.EnQueue(tempB);
    }
}

[[noreturn]] void processDataC(){
    int parity;
    while(data_a_buffer.Empty()){}
    parity = data_a_buffer.DeQueue().a & 1;
    while(true){
        while(data_b_buffer.Empty()){
            usleep(1);
        }
        DataB tempB = data_b_buffer.DeQueue();
        double sum_b = 0;
        for(auto num : tempB.data){
            sum_b += num;
        }
        if (!data_a_buffer.Empty()){
            parity = data_a_buffer.DeQueue().a & 1;
            add_count_a ++;
        }
        // 计算
        sum_b_all += (parity ? sum_b : -sum_b);
        add_count_b++;
    }
}

[[noreturn]]void printNum(){
    int time = 0;
    while (true){
        cout << "时间：" << time << "     收到B数据个数：" << add_count_b << "     收到A数据个数："<< add_count_a << "     当前计算结果：" << sum_b_all << endl;
        fflush(stdout);
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