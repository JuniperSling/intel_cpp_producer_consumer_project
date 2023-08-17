# Intel C++ 项目报告

### 哈尔滨工业大学

### Milagro

### 2022年12月27日



### 一、项目描述

现有两个生产者线程*A*和*B*，一个消费者线程*C*，打印线程*D*。 

- A 和 B 负责生产数据
  - A 线程生产的数据是结构`DataA` ，` DataA.a` 是随机产生的 1 到 100 间的一个整数
  - B 线程生产的数据是结构体 `DataB` ，`DataB.data` 是随机产生的0到1之间的 5 个小数(精确到小数点后 2 位)

- C 负责计算最新收取的 `DataB.data `中数据的加和 `sum_b` 。 `sum_b` 满足当最新收取的 `DataA.a` 数据为奇数时, `sum_b` 取正;当最新收取的 `DataA.a` 数据为偶数时， `sum_b` 取负。如果连续来的数 据都为 `DataB` 时， `sum_b `的正负由最近一次收取到的 `DataA.a `的奇偶决定。 线程 C 会实时维护 一个累计 `sum_b` 的加和 `sum_b_all` 。

- 线程 D 每隔 1 秒钟打印一次当前 sum_b_all 的值

**设计程序， 使得线程间通讯的延迟最小**

```c++
// DataA 和 DataB 定义如下: 
struct DataA {
		int a; 
};
struct DataB{
    double data[5];
};
```

### 二、开发环境

- 系统：Ubuntu 20.04.1 LTS
- 设备：MacBook Pro (13-inch, 2018, Four Thunderbolt 3 Ports)
- CPU：Intel Core i5-8259U CPU @2.30GHz $\times 2$
- 编译器：gcc (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0

### 三、代码思路

#### 1.  双生产者-单消费者模式，使用队列结构，共享缓存数据

<img src="https://milagro-pics.oss-cn-beijing.aliyuncs.com/img/Screenshot%202022-02-26%20at%2010.47.03%20PM.png" alt="Screenshot 2022-02-26 at 10.47.03 PM" style="zoom:50%;" />

由于A和B产生的数据类型不同，使用自定义结构 `DataC` 进一步封装，放入同一个缓存队列。

```c++
struct DataC{
    bool is_type_b; // 0代表a, 1代表b
    int a;
    double data[5];
};
const int MAX_DATA_CACHE_LENGTH = 256;
queue<DataC> data_buffer;
```

==生产者A代码==

```c++
[[noreturn]] void produceA(){
    while (true){
        unique_lock<mutex> lock_producer_a(m); // 加锁
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
```

采用`std::unique_lock`包装互斥锁`std::mutex`。A首先加锁，检查`data_buffer`已满则直接释放锁，等待消费者唤醒。否则将生产数据放入缓冲区。再于析构函数中自动释放锁。

==生产者B代码（节选）==

```c++
tempC.is_type_b = true;
for(double & i : tempC.data){
	i = static_cast<int>(random() % 100) / 100.0;
}
data_buffer.push(tempC);
```

与生产者A竞争互斥锁，节选代码仅展示随机数生成逻辑。

==消费者C代码（节选）==

首先加锁取出数据，**在处理数据之前直接释放锁，防止长时间占有耽误效率**，之后唤醒等待中的生产者线程

```c++
// 取出
unique_lock<mutex> locker_consumer(m); // 加锁
while(data_buffer.empty()){
	cond_consumer.wait(locker_consumer); // 遇到空缓存等待唤醒
}
DataC tempC = data_buffer.front();
data_buffer.pop();
locker_consumer.unlock(); // 释放锁
cond_producer.notify_all(); // 唤醒生产者
```

之后处理数据，根据数据种类是A还是B采取不同操作。在取数据A的过程中，**采用按位与`parity = data & 1`转化为位操作的技巧，规避模2的计算开销**。

```c++
// 是类型A，则计算奇偶性 parity
parity = tempC.a & 1;
```

==线程效率测试==

<img src="https://milagro-pics.oss-cn-beijing.aliyuncs.com/img/Screenshot%202022-02-26%20at%209.51.39%20PM.png" alt="Screenshot 2022-02-26 at 9.51.39 PM" style="zoom:50%;" />

选择 `128` 作为两个缓冲区队列长度，10秒钟约处理`3,100,000`次加法，其中A数据产生比B快。



#### 2.  优化：单消费者-单生产者模式，A和B各自使用两个缓存队列值

<img src="https://milagro-pics.oss-cn-beijing.aliyuncs.com/img/Screenshot%202022-02-26%20at%2010.53.32%20PM.png" alt="Screenshot 2022-02-26 at 10.53.32 PM" style="zoom:50%;" />

==数据缓冲区==

```c++
const int MAX_DATA_A_CACHE_LENGTH = 128;
const int MAX_DATA_B_CACHE_LENGTH = 128;
queue<DataA> data_a_buffer;
queue<DataB> data_b_buffer;
```

A生产数据放进`data_a_buffer`，B生产结构体被放入`data_b_buffer`，消费者从两个缓冲区中分别取数据。

==生产者A代码==

```c++
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
```

这里的区别是**生产者A不再需要通知消费者C**，因为当消费者遇到 buffer A为空的时候不会等待，直接继续计算。

==生产者B代码==

代码没有太大改动，略。

B首先加锁，检查`data_b_buffer`已满则直接释放锁，等待消费者唤醒。否则生产数据放入缓冲区，并且通知等待中的消费者线程C。再于析构函数中自动释放锁

==消费者C代码（节选）==

C启动时首先要决定接收到的B的数据正负，所以C进入消费者循环`while(true){}`前需要先等待来自A的启动数据。在判断A中数据奇偶性时，可以采用按位与`parity = data & 1`的技巧，规避模2的计算开销。

```c++
while(data_a_buffer.empty()){} // 等待A的数据
unique_lock<mutex> locker_consumer_a(mutex_a);
parity = data_a_buffer.front().a & 1; // 取出A的第一个数据，计算奇偶性
data_a_buffer.pop();
locker_consumer_a.unlock();
```

进入处理数据循环`while(true){}`后，要考虑**先取数据A还是数据B**的问题。由于取数据A时允许A的缓冲区为空，因此如果先取A后取B的话可能导致A缓存为空，使用之前的数据，而在取B数据时遇到空缓存区导致线程被阻塞，一旦这段时间内A缓冲区有所更新，那么在后续计算过程中A的数据实时性受到影响。因此，**采用先取到B，后取A的方式，可以避免可能存在的线程A缓存为空，数据未更新问题**。

在取出数据B的过程中，**只在取结构体前后加锁，在锁外的区域计算浮点数求和**，从而避免持有锁时求和，影响效率。

```c++
// 取出B
unique_lock<mutex> locker_consumer_b(mutex_b); // 对B加锁
while(data_b_buffer.empty()){
cond_consumer_b.wait(locker_consumer_b); // 空缓存等待唤醒
}
DataB tempB = data_b_buffer.front(); // 取出结构体
data_b_buffer.pop();
cond_producer_b.notify_one(); // 唤醒等待的生产者线程B
locker_consumer_b.unlock(); // 解锁
// 释放锁后对tempB结构体计算求和，代码略
```

==线程效率测试==

![Screenshot 2022-02-26 at 9.15.21 PM](https://milagro-pics.oss-cn-beijing.aliyuncs.com/img/Screenshot%202022-02-26%20at%209.15.21%20PM.png)

经过测试，选择 `128` 作为两个缓冲区队列长度，10秒钟约处理`7,000,000`次加法



#### 3. 进一步优化：使用环形队列实现无锁编程

<img src="https://milagro-pics.oss-cn-beijing.aliyuncs.com/img/Screenshot%202022-02-26%20at%2010.59.42%20PM.png" alt="Screenshot 2022-02-26 at 10.59.42 PM" style="zoom:30%;" />

`Linux` 内核的 `kfifo`结构是采用了`环形缓冲区`来实现 `FIFO` 队列。采用环形缓冲区的好处为，**当一个数据元素被用掉后，其余数据元素不需要移动其存储位置，从而减少拷贝提高效率**。基于`kfifo`的思想。**由于生产者和消费者分别访问前后两个指针，单生产者单消费者模式不再需要加锁同步**。

==模版编程自定义环形队列==

```c++
const int QueueSize = 256;
template <typename T> class CirQueue{...};
CirQueue<DataA> data_a_buffer;
CirQueue<DataB> data_b_buffer;
```

这里自己实现了一个简单的环形模版`template <typename T> class CirQueue`，将A和B产生的数据分别存放于模版实例化的两个缓存队列中。

==消费者C代码==

```c++
[[noreturn]] void processDataC(){
    int parity;
    while(data_a_buffer.Empty()){}
    parity = data_a_buffer.DeQueue().a & 1;
    while(true){
        while(data_b_buffer.Empty()){
            usleep(1); // 遇到空缓冲区，等待
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
```

此时的生产和消费线程**不再需要互斥加锁以及变量通信**，只需要检查缓冲区中是否有空位/有数据即可。



==线程效率测试==

![Screenshot 2022-02-26 at 8.51.31 PM](https://milagro-pics.oss-cn-beijing.aliyuncs.com/img/Screenshot%202022-02-26%20at%208.51.31%20PM.png)

选择`256`作为两个环形队列缓冲区的数据长度，具有更好的效果，10秒钟约处理`28,000,000`次加法

### 四、总结

我比较了三个处理这道题目的思路：

1. ==双生产者-单消费者==模型，A和B线程的数据占用同一个缓存队列，C线程从队列中依次取数，三个线程彼此互斥，生产者在遇到缓冲区满时等待消费者唤醒，消费者在遇到缓冲区满时等待生产者唤醒。在10秒钟内一共进行了约**3,100,000** 次加法计算。**这种方法需要格外封装一次数据，也可以考虑采用管道通信的方式。**
2. ==单生产者-单消费者==模型，A和B线程的数据各自占用一个缓存队列，C线程从队列中依次取A和B数据，**避免了A和B线程之间的竞争**，在10秒内约进行了 **7,000,000** 次加法计算。
3. ==环形队列无锁编程==，在单对单 P&C模型的基础上继续优化，**减少数据拷贝移动，实现无锁编程**，在10秒内约进行了 **28,000,000** 次加法计算。

<img src="https://milagro-pics.oss-cn-beijing.aliyuncs.com/img/Picture%201.png" alt="Picture 1" style="zoom:30%;" />

目前三种方案比较下来，第一种方案效率最慢，但是模型清晰简单安全，并且可以扩展到更多生产者-消费者的情况，后续的方案更加追求计算速度。

如果希望继续提高速度，可以考虑进一步模仿`kfifo`的设计思想，规定环形缓冲区**必须为2的次幂，通过位运算代替环形缓冲的取模运算**，实现更加快速的出入队列操作。
