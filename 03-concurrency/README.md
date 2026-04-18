# 阶段三：并发编程

## 1. 线程基础

```cpp
#include <thread>
#include <iostream>

void task(int id) {
    std::cout << "Task " << id << " running\n";
}

int main() {
    std::thread t1(task, 1);
    std::thread t2(task, 2);
    
    t1.join();  // 等待线程结束
    t2.join();
    
    return 0;
}

// Ceph大量使用多线程，每个OSD/MON/MDS都是多线程服务
```

## 2. 互斥量与锁

### 2.1 std::mutex

```cpp
#include <mutex>
#include <vector>

class ThreadSafeVector {
    std::vector<int> data;
    std::mutex mtx;
    
public:
    void push_back(int value) {
        std::lock_guard<std::mutex> lock(mtx);  // 自动加锁解锁
        data.push_back(value);
    }
    
    int get(size_t index) {
        std::lock_guard<std::mutex> lock(mtx);
        return data.at(index);
    }
};

// Ceph: src/common/ceph_mutex.h 定义了多种锁类型
```

### 2.2 std::unique_lock（更灵活）

```cpp
std::mutex mtx;
{
    std::unique_lock<std::mutex> lock(mtx);
    // ... 操作 ...
    lock.unlock();  // 可以提前解锁
    
    lock.lock();    // 可以再次加锁
}  // 自动解锁
```

### 2.3 std::recursive_mutex（递归锁）

```cpp
std::recursive_mutex rmtx;

void recursive_func(int depth) {
    std::lock_guard<std::recursive_mutex> lock(rmtx);
    if (depth > 0) {
        recursive_func(depth - 1);  // 同一线程可以重复获取锁
    }
}
```

## 3. 条件变量

```cpp
#include <condition_variable>
#include <queue>

template<typename T>
class ThreadSafeQueue {
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;
    
public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(std::move(value));
        }
        cv.notify_one();  // 通知等待的线程
    }
    
    T pop() {
        std::unique_lock<std::mutex> lock(mtx);
        // 等待直到队列不为空
        cv.wait(lock, [this] { return !queue.empty(); });
        
        T value = std::move(queue.front());
        queue.pop();
        return value;
    }
};

// Ceph: src/msg/Messenger.h 消息队列使用类似机制
```

## 4. 原子操作

```cpp
#include <atomic>

std::atomic<int> counter(0);
std::atomic<bool> running(true);

// 原子操作
counter.fetch_add(1);       // 原子加
counter++;                  // 原子自增
counter.load();             // 原子读取
counter.store(10);          // 原子写入

// 比较交换（CAS）
int expected = 10;
bool success = counter.compare_exchange_strong(expected, 20);

// Ceph: src/common/RefCountedObj.h 使用atomic实现引用计数
// 实际类型是 std::atomic<uint64_t> nref{1}
```

## 5. 线程安全设计模式

### 5.1 读写锁模式

```cpp
#include <shared_mutex>

class ThreadSafeCache {
    std::map<std::string, std::string> cache;
    mutable std::shared_mutex mtx;
    
public:
    std::string get(const std::string& key) const {
        std::shared_lock<std::shared_mutex> lock(mtx);  // 读锁，允许多读
        auto it = cache.find(key);
        return it != cache.end() ? it->second : "";
    }
    
    void set(const std::string& key, const std::string& value) {
        std::unique_lock<std::shared_mutex> lock(mtx);  // 写锁，独占
        cache[key] = value;
    }
};
```

### 5.2 双重检查锁定

```cpp
class Singleton {
    static Singleton* instance;
    static std::mutex mtx;
    
public:
    static Singleton* get() {
        if (!instance) {                         // 第一次检查
            std::lock_guard<std::mutex> lock(mtx);
            if (!instance) {                     // 第二次检查
                instance = new Singleton();
            }
        }
        return instance;
    }
};

// C++11后更推荐使用静态局部变量
class Singleton {
public:
    static Singleton& get() {
        static Singleton instance;
        return instance;
    }
};
```

## 6. call_once

```cpp
#include <mutex>

class Resource {
    static std::once_flag init_flag;
    static Resource* instance;
    
public:
    static Resource* get() {
        std::call_once(init_flag, [] {
            instance = new Resource();
        });
        return instance;
    }
};
```

---

# 无锁并发（Lock-Free）

> 无锁编程是高并发系统的核心技术，Ceph在高性能路径上使用了无锁技术。

## 7. 为什么需要无锁？

### 7.1 锁的问题

```cpp
// 有锁编程的问题
std::mutex mtx;
int counter = 0;

void increment() {
    std::lock_guard<std::mutex> lock(mtx);  // 可能阻塞
    counter++;
}

// 问题：
// 1. 锁竞争：多线程等待同一把锁，性能下降
// 2. 上下文切换：阻塞线程需要切换
// 3. 死锁风险：锁顺序不当
// 4. 优先级反转：低优先级线程持有锁，高优先级线程等待
// 5. 无法用于信号处理程序
```

### 7.2 无锁的优势

```cpp
// 无锁编程
std::atomic<int> counter(0);

void increment() {
    counter.fetch_add(1, std::memory_order_relaxed);  // 不阻塞
}

// 优势：
// 1. 不会阻塞，没有线程等待
// 2. 不会死锁
// 3. 更好的缓存一致性
// 4. 适合高竞争场景
```

## 8. C++内存模型基础

### 8.1 内存序（Memory Order）

这是无锁编程的**核心概念**。理解它才能正确写出无锁代码。

```cpp
// 六种内存序（从弱到强）
std::memory_order_relaxed    // 最弱：只保证原子性，不保证顺序
std::memory_order_consume    // C++17弃用
std::memory_order_acquire    // 获取：读操作屏障
std::memory_order_release    // 释放：写操作屏障
std::memory_order_acq_rel    // 获取-释放：读写都屏障
std::memory_order_seq_cst    // 最强：顺序一致性（默认）
```

### 8.2 Happens-Before关系

```cpp
// 理解"先于"关系
int x = 0;
int y = 0;

// 线程1
x = 1;                    // A
y.store(1, release);      // B (release)

// 线程2
while (y.load(acquire) != 1);  // C (acquire)  
assert(x == 1);          // D - 保证看到x=1！

// 因为：
// B synchronizes-with C (release-acquire配对)
// A happens-before B (程序顺序)
// C happens-before D (程序顺序)
// 所以 A happens-before D，能看到x=1
```

### 8.3 各种内存序详解

```cpp
#include <atomic>

// 1. relaxed：只保证原子性，最宽松
std::atomic<int> counter(0);
counter.fetch_add(1, std::memory_order_relaxed);
// 适用：简单计数，不关心其他变量

// 2. acquire/release：经典生产者-消费者
std::atomic<bool> ready(false);
int data = 0;

// 生产者
void produce() {
    data = 42;                              // 准备数据
    ready.store(true, std::memory_order_release);  // 发布
}

// 消费者
void consume() {
    while (!ready.load(std::memory_order_acquire));  // 等待
    assert(data == 42);                     // 保证看到data=42
}

// 3. seq_cst：默认，最强，顺序一致
std::atomic<int> x(0);
x.store(1);  // 等价于 x.store(1, std::memory_order_seq_cst);

// 所有线程看到相同顺序的操作
// 简单但性能最差
```

### 8.4 连续一致性（Sequential Consistency）

```cpp
// seq_cst 的例子
std::atomic<int> x(0), y(0);

// 线程1          线程2
x.store(1);      y.store(1);
int r1 = y.load();   int r2 = x.load();

// 不可能出现 r1==0 && r2==0
// 因为 seq_cst 保证全局顺序
```

## 9. CAS（Compare-And-Swap）

### 9.1 基础用法

```cpp
std::atomic<int> value(0);

// CAS操作：如果值等于expected，则设为desired
int expected = 0;
bool success = value.compare_exchange_strong(expected, 1);
// 如果value==0，设为1，返回true
// 如果value!=0，expected被更新为当前值，返回false

// weak版本：可能假失败，但在循环中更高效
int expected = 0;
while (!value.compare_exchange_weak(expected, 1)) {
    // expected已被更新为当前值
    // 重试
}
```

### 9.2 无锁计数器

```cpp
class LockFreeCounter {
    std::atomic<int> counter{0};
    
public:
    void increment() {
        // 方法1：fetch_add（推荐）
        counter.fetch_add(1, std::memory_order_relaxed);
        
        // 方法2：CAS循环（学习用）
        int old_val = counter.load(std::memory_order_relaxed);
        while (!counter.compare_exchange_weak(
            old_val, old_val + 1,
            std::memory_order_relaxed,
            std::memory_order_relaxed)) {
            // CAS失败，auto retry
        }
    }
    
    int get() const {
        return counter.load(std::memory_order_relaxed);
    }
};
```

## 10. 无锁数据结构

### 10.1 无锁栈（Lock-Free Stack）

```cpp
template<typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;
    };
    
    std::atomic<Node*> head{nullptr};
    
public:
    void push(const T& value) {
        Node* new_node = new Node{value, nullptr};
        new_node->next = head.load(std::memory_order_relaxed);
        
        // CAS: 如果head还是new_node->next，则设为new_node
        while (!head.compare_exchange_weak(
            new_node->next, new_node,
            std::memory_order_release,
            std::memory_order_relaxed)) {
            // head被其他线程修改了，重试
        }
    }
    
    bool pop(T& result) {
        Node* old_head = head.load(std::memory_order_relaxed);
        
        while (old_head && !head.compare_exchange_weak(
            old_head, old_head->next,
            std::memory_order_acquire,
            std::memory_order_relaxed)) {
            // head被修改，重试
        }
        
        if (old_head) {
            result = std::move(old_head->data);
            // 注意：这里不能立即delete，见ABA问题
            return true;
        }
        return false;
    }
};

// 用法
LockFreeStack<int> stack;
stack.push(1);
stack.push(2);
int val;
stack.pop(val);  // val = 2
```

### 10.2 无锁队列（MPSC - 多生产者单消费者）

```cpp
// 简化的无锁队列（多生产者，单消费者）
template<typename T>
class MPSCQueue {
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};
    };
    
    std::atomic<Node*> head{nullptr};  // 生产者端
    Node* tail = nullptr;               // 消费者端（单线程）
    
public:
    // 多线程安全的push
    void push(const T& value) {
        Node* new_node = new Node{value, nullptr};
        Node* old_head = head.exchange(new_node, std::memory_order_acq_rel);
        old_head->next.store(new_node, std::memory_order_release);
    }
    
    // 单线程pop
    bool pop(T& result) {
        if (tail && tail->next.load(std::memory_order_acquire)) {
            Node* next = tail->next.load(std::memory_order_acquire);
            result = std::move(next->data);
            delete tail;
            tail = next;
            return true;
        }
        
        Node* h = head.load(std::memory_order_acquire);
        if (h == nullptr) return false;
        
        if (tail == nullptr) {
            tail = h;
        }
        
        if (tail->next.load(std::memory_order_acquire)) {
            Node* next = tail->next.load(std::memory_order_acquire);
            result = std::move(next->data);
            delete tail;
            tail = next;
            return true;
        }
        return false;
    }
};
```

### 10.3 无锁单链表（Lock-Free List）

```cpp
template<typename T>
class LockFreeList {
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};
    };
    
    std::atomic<Node*> head{nullptr};
    
public:
    void insert(const T& value) {
        Node* new_node = new Node{value, nullptr};
        Node* old_head = head.load(std::memory_order_relaxed);
        
        do {
            new_node->next.store(old_head, std::memory_order_relaxed);
        } while (!head.compare_exchange_weak(
            old_head, new_node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }
    
    bool find(const T& value) const {
        Node* current = head.load(std::memory_order_acquire);
        while (current) {
            if (current->data == value) return true;
            current = current->next.load(std::memory_order_acquire);
        }
        return false;
    }
};
```

## 11. ABA问题

### 11.1 问题描述

```cpp
// ABA问题：值从A变到B，再变回A，CAS无法检测

// 线程1                    线程2
// head -> A -> B -> C
intptr_t expected = (intptr_t)head.load();
                            // 删除A，删除B
                            // head -> C
                            // 重用A的内存，放入新节点D
                            // head -> C
                            // 此时内存中的A指针位置可能被重用
                            
// CAS成功！但链表已损坏
compare_exchange_strong(expected, new_node);
```

### 11.2 解决方案：带版本号

```cpp
// 方案1：双字CAS（需要硬件支持128位CAS）
struct TaggedPointer {
    Node* ptr;
    uint64_t version;
};

std::atomic<TaggedPointer> head;

// 方案2：使用 hazard pointer 或 RCU
// 方案3：引用计数

// 带版本号的包装
template<typename T>
class TaggedPointer {
    std::atomic<std::pair<T*, uint64_t>> ptr_and_version;
    
public:
    T* load() {
        auto [ptr, ver] = ptr_and_version.load();
        return ptr;
    }
    
    bool compare_exchange(T* expected, T* desired) {
        auto current = ptr_and_version.load();
        uint64_t new_version = current.second + 1;
        return ptr_and_version.compare_exchange_strong(
            current,
            {desired, new_version}
        );
    }
};
```

## 12. 内存屏障（Memory Fence）

### 12.1 手动屏障

```cpp
// 有时候需要在特定位置插入屏障
std::atomic<int> x(0), y(0);

// 写线程
x.store(1, std::memory_order_relaxed);
std::atomic_thread_fence(std::memory_order_release);  // 手动屏障
y.store(1, std::memory_order_relaxed);

// 读线程
while (y.load(std::memory_order_relaxed) != 1);
std::atomic_thread_fence(std::memory_order_acquire);  // 手动屏障
assert(x.load(std::memory_order_relaxed) == 1);
```

### 12.2 常见屏障类型

```cpp
// Release屏障：之前的写不能重排到这里之后
std::atomic_thread_fence(std::memory_order_release);

// Acquire屏障：之后的读不能重排到这里之前
std::atomic_thread_fence(std::memory_order_acquire);

// Full屏障：完全屏障
std::atomic_thread_fence(std::memory_order_seq_cst);
```

## 13. Ceph中的无锁应用

### 13.1 引用计数（RefCounted）

```cpp
// src/common/RefCountedObj.h — Ceph引用计数基类（简化）
class RefCountedObject {
    mutable std::atomic<uint64_t> nref{1};  // 注意：uint64_t，不是 int

public:
    const RefCountedObject *get() const { _get(); return this; }
    RefCountedObject *get() { _get(); return this; }
    void put() const;  // _get() 和 put() 在 .cc 文件中实现
private:
    void _get() const { nref.fetch_add(1, std::memory_order_relaxed); }
};
// put() 内部：nref.fetch_sub(1, memory_order_acq_rel)，如果归零则 delete this
```

### 13.2 条件变量+互斥锁队列（非无锁）

```cpp
// src/common/Finisher.h — 异步完成任务队列
// 注意：这不是无锁队列！它使用 mutex + condition_variable
class Finisher {
    ceph::mutex finisher_lock;
    ceph::condition_variable finisher_cond;
    std::vector<Context*> finisher_queue;  // 用 vector 存任务
    // ...
};

// Ceph 中真正的 atomic 操作见：
// - ceph::atomic<T>（src/include/ceph_assert.h 中定义）
// - buffer::raw::nref（引用计数，src/include/buffer_raw.h）
```

### 13.3 Crimson的share-nothing设计

```cpp
// Ceph 的 Crimson（下一代 OSD 前端）基于 Seastar 框架
// 采用 share-nothing 设计：每个核心有自己的内存区域，不需要锁
// 核心间通过消息传递通信，而不是共享内存+锁
//
// 源码位置：src/crimson/ （不是 src/seastar/）
// Seastar 是外部依赖，Ceph 通过 Crimson 适配层使用它
```

## 14. 无锁编程注意事项

### 14.1 何时使用无锁

```cpp
// 适合无锁：
// 1. 引用计数
// 2. 简单计数器
// 3. 单生产者单消费者队列
// 4. 状态标志

// 不适合无锁：
// 1. 复杂数据结构（用锁更简单）
// 2. 需要事务性操作
// 3. 团队缺乏无锁编程经验（正确性难保证）
```

### 14.2 调试技巧

```cpp
// 1. 使用ThreadSanitizer检测数据竞争
// 编译: g++ -fsanitize=thread -g

// 2. 使用内存序验证工具
// TSAN_OPTIONS="report_data_races=1"

// 3. 压力测试
void stress_test() {
    const int N = 1000000;
    std::atomic<int> counter(0);
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&] {
            for (int j = 0; j < N; j++) {
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    for (auto& t : threads) t::join();
    
    assert(counter.load() == 10 * N);
}
```

### 14.3 性能考量

```cpp
// 无锁不代表更快

// 锁的优势：
// - 简单正确
// - 低竞争时性能好
// - 内核可优化等待

// 无锁的优势：
// - 高竞争时无阻塞
// - 无死锁风险
// - 可用于信号处理

// 实践建议：
// 1. 先用正确性优先的锁实现
// 2. 使用性能测试证明瓶颈
// 3. 只有在必要时才无锁优化
// 4. 使用标准库的无锁实现
```

## 练习

参考 `exercises/` 目录：

1. 实现线程安全的计数器
2. 实现生产者-消费者队列
3. 实现简单的线程池
4. **新增：实现无锁栈**
5. **新增：实现CAS计数器的两种方式**

## Ceph源码阅读建议

```
src/common/ceph_mutex.h          # Ceph锁封装
src/common/Thread.h              # 线程封装
src/common/RefCountedObj.h       # 原子引用计数（std::atomic<uint64_t>）
src/common/Finisher.h            # 异步完成队列（mutex+condvar，非无锁）

# 无锁相关
src/include/buffer_raw.h         # buffer::raw::nref 原子引用计数
src/crimson/                     # Crimson: share-nothing无锁设计
```