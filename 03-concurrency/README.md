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

#### 理解 lock_guard 的语法

`mtx.lock()` 是函数调用，但 `std::lock_guard<std::mutex> lock(mtx)` 是在**声明一个局部变量**：

```cpp
std::lock_guard<std::mutex>  lock(mtx);
^--------------------------^ ^---^ ^---^
        类型（类名）        变量名  构造参数
```

**为什么声明变量就自动加锁了？** 因为 C++ 的类有构造函数和析构函数：

```cpp
// std::lock_guard 的简化实现（伪代码）
template<class Mutex>
class lock_guard {
    Mutex& m;
public:
    lock_guard(Mutex& mtx) : m(mtx) {
        m.lock();    // ← 构造时自动加锁
    }
    ~lock_guard() {
        m.unlock();  // ← 销毁时自动解锁
    }
};
```

声明 `lock` 变量时 → 调用构造函数 → 自动执行 `mtx.lock()`
函数结束 `lock` 变量销毁时 → 调用析构函数 → 自动执行 `mtx.unlock()`

这就是 C++ 的 **RAII** 惯用法：用对象生命周期管理资源。

#### 为什么用 lock_guard 而不是 mtx.lock()？

**核心原因：异常安全和防遗忘。**

```cpp
// 危险写法：手动 lock/unlock
void bad() {
    mtx.lock();
    if (error) return;          // BUG！忘记 unlock，死锁
    do_something();             // 如果抛异常，也死锁
    mtx.unlock();
}

// 正确写法：lock_guard（RAII）
void good() {
    std::lock_guard<std::mutex> lock(mtx);  // 构造时 lock
    if (error) return;          // ✅ 析构自动 unlock
    do_something();             // ✅ 抛异常也自动 unlock
}                               // ✅ 离开作用域自动 unlock
```

**常见疑问：多次调用 push_back 会不会因锁未释放而卡住？**

**不会。`lock_guard` 的生命周期是函数调用，不是对象生命周期。**

```cpp
ThreadSafeVector vec;
vec.push_back(1);  // 创建 lock → 加锁 → push → lock 析构 → 解锁
vec.push_back(2);  // 创建新的 lock → 加锁 → push → lock 析构 → 解锁
vec.push_back(3);  // 同上，每次都是独立的 lock 对象
```

- `lock` 是 `push_back` 的**局部变量**，不是 `ThreadSafeVector` 的成员
- 每次调用都会创建新的 `lock_guard`，函数返回时它就析构了
- `vec` 对象没销毁 ≠ `lock` 没销毁，`lock` 在函数结束时就死了

**Ceph 源码中几乎全部使用 `lock_guard`（或封装的 `ceph::lock_guard`），极少直接调用 `lock()`/`unlock()`。**

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

**`unique_lock` 比 `lock_guard` 灵活在哪？** 看简化实现：

```cpp
// lock_guard：只有一个引用，构造后无法控制
template<class Mutex>
class lock_guard {
    Mutex& m;  // 只存引用，没有状态
public:
    lock_guard(Mutex& mtx) : m(mtx) { m.lock(); }
    ~lock_guard() { m.unlock(); }
    // 没有 lock()/unlock() 方法！
};

// unique_lock：多了一个 owns_lock 状态标记
template<class Mutex>
class unique_lock {
    Mutex* m;          // 指针，可以为 nullptr
    bool owns_lock;    // 标记当前是否持有锁
public:
    unique_lock(Mutex& mtx) : m(&mtx), owns_lock(true) {
        m->lock();
    }
    ~unique_lock() {
        if (owns_lock) m->unlock();  // 只有持有锁时才解锁
    }
    void unlock() {
        m->unlock();
        owns_lock = false;           // 标记已释放
    }
    void lock() {
        m->lock();
        owns_lock = true;            // 标记已获取
    }
    bool owns_lock() const { return owns_lock; }
};
```

**关键差异：`lock_guard` 没有状态标记，所以无法手动控制；`unique_lock` 用 `owns_lock` 标记跟踪锁的状态。**

#### unique_lock 的四种构造方式

`unique_lock` 的灵活性主要来自构造时可以指定不同的策略：

```cpp
std::mutex mtx;

// 1. 默认：构造时加锁（和 lock_guard 一样）
std::unique_lock<std::mutex> lk1(mtx);

// 2. defer_lock：构造时不加锁，稍后手动加锁
std::unique_lock<std::mutex> lk2(mtx, std::defer_lock);
lk2.lock();  // 需要时再锁

// 3. try_to_lock：构造时尝试加锁，失败不阻塞
std::unique_lock<std::mutex> lk3(mtx, std::try_to_lock);
if (lk3.owns_lock()) {
    // 成功获取锁
} else {
    // 锁被别人持有，但不阻塞
}

// 4. adopt_lock：假定已经加锁，只接管管理权
mtx.lock();
std::unique_lock<std::mutex> lk4(mtx, std::adopt_lock);
// 析构时会 unlock，但构造时不再 lock
```

**对应简化实现：**

```cpp
template<class Mutex>
class unique_lock {
    Mutex* m;
    bool owns;
public:
    // 默认构造：加锁
    unique_lock(Mutex& mtx) : m(&mtx), owns(true) {
        m->lock();
    }
    // defer_lock：不加锁，只绑定
    unique_lock(Mutex& mtx, std::defer_lock_t) : m(&mtx), owns(false) {}
    // try_to_lock：尝试加锁，失败不阻塞
    unique_lock(Mutex& mtx, std::try_to_lock_t) : m(&mtx), owns(m->try_lock()) {}
    // adopt_lock：已加锁，只接管
    unique_lock(Mutex& mtx, std::adopt_lock_t) : m(&mtx), owns(true) {}
};
```

#### unique_lock 其他重要特性

```cpp
std::mutex mtx;

// 1. 可以转移所有权（move），lock_guard 不行
std::unique_lock<std::mutex> lk1(mtx);
std::unique_lock<std::mutex> lk2 = std::move(lk1);  // 锁的所有权转移给 lk2
// lk1 此后为空，不再管理锁

// 2. release()：放弃管理权，不解锁
std::unique_lock<std::mutex> lk(mtx);
mtx.unlock();              // 需要手动解锁！
// lk 析构时不会再 unlock

// 3. 配合 condition_variable（这是 unique_lock 最常见的场景）
std::condition_variable cv;
std::unique_lock<std::mutex> lk(mtx);
cv.wait(lk, []{ return ready; });  // wait 要求传 unique_lock，不能传 lock_guard
// 原因：wait 内部会 unlock → 等待 → 重新 lock，lock_guard 做不到
```

**使用场景：** 配合条件变量 `condition_variable::wait()` 时需要反复解锁/加锁，必须用 `unique_lock`。

### 2.3 std::recursive_mutex（递归锁）

```cpp
std::recursive_mutex rmtx;  // 注意：这是一个独立的类型，不是 mutex 的子类

void recursive_func(int depth) {
    std::lock_guard<std::recursive_mutex> lock(rmtx);
    // ↑ 第一次调用：lock 构造时调用 rmtx.lock()，计数=1
    //   递归调用时：lock 构造时调用 rmtx.lock()，同一线程，计数+1=2
    //   再递归调用：计数+1=3 ...
    //   每层返回时 lock 析构，计数-1，直到计数归零才真正释放锁
    if (depth > 0) {
        recursive_func(depth - 1);  // 同一线程可以重复获取锁
    }
}
```

**普通 `mutex` 同一线程第二次 `lock()` 会死锁，`recursive_mutex` 为什么不会？**

关键在于 `recursive_mutex` 内部记录了**谁持有锁**和**锁了几次**：

```cpp
// 普通 mutex：只有"锁/未锁"两种状态
struct mutex {
    bool locked;           // true 或 false
    // lock()：如果 locked == true，不管是谁锁的，一律阻塞
    // 同一线程第二次 lock() → locked 已经是 true → 阻塞自己 → 死锁
};

// recursive_mutex：记录"谁锁的"和"锁了几次"
struct recursive_mutex {
    pthread_t owner;      // 持有锁的线程 ID
    int count;            // 重入计数
    // ... 底层等待队列 ...
    
    void lock() {
        if (owner == current_thread()) {
            count++;     // 同一线程，直接计数+1，不阻塞！
            return;
        }
        // 不同线程，和普通 mutex 一样阻塞等待
        wait_until_unlocked(this);
        owner = current_thread();
        count = 1;
    }
    
    void unlock() {
        count--;
        if (count == 0) {
            owner = 0;
            wake_up_waiter(this);
        }
    }
};
```

**`std::recursive_mutex` 在 C++ 层没有实现递归逻辑！** 它只是初始化了一个带 `PTHREAD_MUTEX_RECURSIVE` 属性的 `pthread_mutex_t`：

```cpp
// /usr/include/c++/13/mutex — C++ 层只是薄封装
class recursive_mutex : private __recursive_mutex_base {
    // 内部成员：__gthread_recursive_mutex_t _M_mutex;
    // 而 __gthread_recursive_mutex_t 就是 pthread_mutex_t
    
    void lock() {
        // 直接调用 pthread_mutex_lock，没有任何额外逻辑！
        int __e = __gthread_recursive_mutex_lock(&_M_mutex);
        if (__e) __throw_system_error(__e);
    }
};

// /usr/include/x86_64-linux-gnu/c++/13/bits/gthr-posix.h — 初始化时设置递归属性
__gthread_recursive_mutex_init_function(pthread_mutex_t *__mutex) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);  // 关键！
    pthread_mutex_init(__mutex, &attr);
}

// __gthread_recursive_mutex_lock 直接调用 __gthread_mutex_lock
// 递归逻辑完全由 glibc 的 pthread 在内核层实现
```

**总结：**
- `recursive_mutex` 是独立类型，不是 `mutex` 的子类
- C++ 层只做初始化（设置 `PTHREAD_MUTEX_RECURSIVE`）和转发调用
- 递归能力（记录 owner + count）由 glibc/内核提供
- `lock_guard<recursive_mutex>` 每次构造调用 `lock()`，计数+1；析构调用 `unlock()`，计数-1
- **`recursive_mutex` 通常意味着设计有问题，应该优先考虑重构代码避免递归加锁**

**核心机制：`recursive_mutex` 内部维护了 `owner`（持有者线程 ID）和 `count`（重入次数），同一线程重复加锁只需计数+1，不需要阻塞。**

**注意：`recursive_mutex` 通常意味着设计有问题，应该优先考虑重构代码避免递归加锁。**

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

#### 读锁和写锁是怎么实现的？

**关键理解：读写锁的能力来自 `shared_mutex`，不是来自 `shared_lock` / `unique_lock`。**

`shared_lock` 和 `unique_lock` 只是 RAII 包装器，它们调用的是互斥量的**不同方法**：

```cpp
// shared_lock 的简化实现：调用 lock_shared / unlock_shared
template<class Mutex>
class shared_lock {
    Mutex* m;
    bool owns;
public:
    shared_lock(Mutex& mtx) : m(&mtx), owns(true) {
        m->lock_shared();      // ← 调用互斥量的 shared 接口
    }
    ~shared_lock() {
        if (owns) m->unlock_shared();
    }
};

// unique_lock 2.2 节已介绍，它调用 lock / unlock
// 当 Mutex = shared_mutex 时：
//   unique_lock 构造时调用 mtx.lock()        → 独占锁
//   unique_lock 析构时调用 mtx.unlock()       → 释放独占锁
//   shared_lock 构造时调用 mtx.lock_shared()  → 共享锁
//   shared_lock 析构时调用 mtx.unlock_shared() → 释放共享锁
```

**读锁和写锁的真正实现靠 `shared_mutex` 内部的 `lock()` 和 `lock_shared()` 两个方法：**

```cpp
// shared_mutex 的简化逻辑（底层由 pthread_rwlock_t 实现）
struct shared_mutex {
    int reader_count;      // 当前持有读锁的线程数
    bool writer_holding;   // 是否有线程持有写锁
    // ... 等待队列 ...
    
    void lock() {           // 写锁（独占）
        // 如果有读者或写者正在持有，阻塞等待
        while (reader_count > 0 || writer_holding) {
            wait();
        }
        writer_holding = true;
    }
    
    void unlock() {         // 释放写锁
        writer_holding = false;
        wake_up_all();
    }
    
    void lock_shared() {    // 读锁（共享）
        // 如果有写者正在持有，阻塞等待
        while (writer_holding) {
            wait();
        }
        reader_count++;     // 读者计数+1，不阻塞其他读者！
    }
    
    void unlock_shared() {  // 释放读锁
        reader_count--;
        if (reader_count == 0) {
            wake_up_writer();  // 最后一个读者离开，唤醒写者
        }
    }
};
```

**规则：**
- 读锁之间**不互斥**：多个线程可以同时 `lock_shared()`
- 写锁之间**互斥**：同时只有一个线程可以 `lock()`
- 读锁和写锁**互斥**：有读者时写者必须等待，有写者时读者必须等待

**C++ 标准库的实际实现：** `shared_mutex` 就是 `pthread_rwlock_t` 的薄封装，和 `recursive_mutex` 是 `pthread_mutex_t` 的薄封装同理。

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
```

#### 为什么需要 acquire/release？relaxed 不够吗？

```cpp
std::atomic<bool> ready(false);
int data = 0;

// 生产者
void produce() {
    data = 42;
    ready.store(true, std::memory_order_relaxed);  // 用 relaxed！
}

// 消费者
void consume() {
    while (!ready.load(std::memory_order_relaxed));  // 用 relaxed！
    // 循环退出时，ready 一定能看到 true（原子性保证）
    // 但 data 可能还是 0！
    assert(data == 42);  // 可能失败！
}
```

**循环退出只保证 `ready == true`，不保证 `data == 42`。** 因为 CPU 和编译器都会做重排序：

1. **编译器重排序**：编译器可能把 `data = 42` 优化掉或移到 `ready.store` 之后
2. **CPU 重排序**：`ready.store` 可能先于 `data = 42` 到达消费者缓存（StoreStore 重排序）

**acquire/release 的作用不是让循环退出，而是建立 happens-before 关系：**

```cpp
// 生产者
void produce() {
    data = 42;                                      // A
    ready.store(true, std::memory_order_release);    // B (release)
}

// 消费者
void consume() {
    while (!ready.load(std::memory_order_acquire));  // C (acquire)
    assert(data == 42);                              // D — 保证看到 data=42
}

// release 保证：A 不会重排到 B 之后（之前的写对消费者可见）
// acquire 保证：D 不会重排到 C 之前（之后的读能看到生产者的写）
// 所以 A happens-before D，data==42 有保证
```

#### acquire/release 和 seq_cst 的差别

```cpp
std::atomic<int> x(0), y(0);

// 线程1                          // 线程2
x.store(1, memory_order_release);  y.store(1, memory_order_release);  // A, C
int r1 = y.load(memory_order_acquire);  int r2 = x.load(memory_order_acquire);  // B, D
```

**用 acquire/release：可能出现 `r1==0 && r2==0`**

acquire/release 只约束了：
- release：A 之前的写不会重排到 A 之后
- acquire：B 之后的读不会重排到 B 之前

**但它没有约束 A 和 B 之间的顺序。** 即 StoreLoad 重排是允许的：

```
程序顺序:  A(store x) → B(load y)
实际执行:  B(load y) → A(store x)    ← acquire/release 允许这种重排！
```

如果两个线程都发生 StoreLoad 重排：

```
线程1 实际执行: B(load y=0) → A(store x=1)
线程2 实际执行: D(load x=0) → C(store y=1)
结果: r1=0, r2=0   ← acquire/release 下这是合法的！
```

**用 seq_cst：不可能出现 `r1==0 && r2==0`**

seq_cst 在 acquire/release 基础上额外禁止了 StoreLoad 重排，保证所有线程看到所有 seq_cst 操作的**同一个全局顺序**。

```
acquire/release:  线程间只保证单向依赖
  线程1: A → B     线程2: C → D
  B 和 C 之间没有约束，可以交叉

seq_cst:          所有线程看到同一个全序
  全局: A → C → B → D  (所有线程认同这个顺序)
```

#### 硬件差异

- **x86（TSO）**：天然禁止 StoreLoad 重排，即使用 `memory_order_acquire` 也不会出现 r1=0 && r2=0
- **ARM / PowerPC**：允许 StoreLoad 重排，用 `memory_order_acquire` **可以** 出现 r1=0 && r2=0

所以如果只在 x86 上测试，acquire/release 和 seq_cst 表现一样，测不出区别。但 C++ 内存模型是按最弱架构定义的，acquire/release 不保证多变量同步场景的正确性。

**简单规则：**
- 单个原子变量的生产者-消费者：`acquire/release` 足够，更快
- 涉及多个原子变量的同步：需要 `seq_cst`，更安全

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
// 无锁队列（多生产者，单消费者）
template<typename T>
class MPSCQueue {
    struct Node {
        T data;
        std::atomic<Node*> next{nullptr};
        Node() = default;               // 哨兵节点构造
        Node(const T& v) : data(v) {}   // 数据节点构造
    };
    
    std::atomic<Node*> head;            // 生产者端
    Node* tail;                          // 消费者端（单线程）
    
public:
    MPSCQueue() {
        Node* sentinel = new Node();     // 哨兵节点，避免空指针
        head.store(sentinel, std::memory_order_relaxed);
        tail = sentinel;
    }
    
    ~MPSCQueue() {
        T val;
        while (pop(val)) {}
        delete tail;                     // 清理剩余哨兵
    }
    
    // 多线程安全的push
    void push(const T& value) {
        Node* new_node = new Node(value);
        Node* old_head = head.exchange(new_node, std::memory_order_acq_rel);
        // old_head 永远不会是 nullptr：至少有哨兵节点
        old_head->next.store(new_node, std::memory_order_release);
    }
    
    // 单线程pop
    bool pop(T& result) {
        Node* next = tail->next.load(std::memory_order_acquire);
        if (next == nullptr) return false;  // 队列为空
        
        result = std::move(next->data);
        Node* old_tail = tail;
        tail = next;                     // next 成为新哨兵
        delete old_tail;                 // 删除旧哨兵（可能是原始哨兵或已消费的数据节点）
        return true;
    }
    
    bool empty() const {
        return tail->next.load(std::memory_order_acquire) == nullptr;
    }
};
```

**为什么需要哨兵节点？**

如果 `head` 初始化为 `nullptr`，第一次 `push` 时 `head.exchange()` 返回 `nullptr`，
然后 `nullptr->next.store(...)` 会段错误。

**哨兵节点的流转过程（关键理解）：**

每个 pop 不会删除刚取出的数据节点，而是删除旧的"哨兵位置"，把取完数据的节点变成新哨兵：
```
初始:  [sentinel] → [A:1] → [B:2] → null
                     ↑tail 指向 sentinel

push(1), push(2) 后:
       [sentinel] → [A:1] → [B:2] → null
        ↑head=B     A.next=B
        ↑tail=sentinel

pop() 取出1: 删除 sentinel，A 成为新哨兵
       [A:1] → [B:2] → null
        ↑head=B
        ↑tail=A  （A的数据已取出，A现在是哨兵position）

pop() 取出2: 删除 A（A是push进来的数据节点！），B 成为新哨兵
       [B:2] → null
        ↑head=B
        ↑tail=B  （B的数据已取出，B现在是哨兵position）
```

**被删除的不仅有原始哨兵，还有 push 进来的数据节点**——它们在下一轮 pop 时作为"旧哨兵"被删除。
队列为空后，tail 指向最后一个已消费节点（充当哨兵），析构时 `delete tail` 释放它。

哨兵节点是一个不存储数据的空节点，始终存在，保证 `exchange` 的返回值不会是 `nullptr`。
`pop` 时跳过哨兵节点，只返回真正的数据节点。

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

ABA问题：值从A变到B，再变回A，CAS只看到"还是A"就认为没变过，实际中间已经天翻地覆。

以无锁栈的 pop 操作为例：

```
初始状态（head 是 atomic<Node*>，直接指向栈顶，没有哨兵节点）:

  head(指针) ──→ [A|next=B] ──→ [B|next=C] ──→ [C|next=null]

==== 线程1：准备 pop A ====

线程1:
  local_old = head.load()      // local_old = 0x1000 (A的地址)
  local_next = local_old->next // local_next = 0x2000 (B的地址)
  // 准备 CAS(head, local_old, local_next)
  // 即：如果 head 还是 0x1000，就改成 0x2000（B的地址）
  // 调度器挂起线程1

==== 线程2：趁线程1暂停，一顿操作 ====

线程2: pop A（head 从 A 改为 B）
  head ──→ [B|next=C] �──→ [C|next=null]
            A 的节点内存（0x1000）被释放

线程2: pop B（head 从 B 改为 C）
  head ──→ [C|next=null]
            A(0x1000) 和 B(0x2000) 的内存都被释放

线程2: push D（分配器恰好复用了 A 的内存地址 0x1000）
  head ──→ [D|next=C] ──→ [C|next=null]
            D 的地址 == 0x1000（和 A 一样，内存复用）

==== 线程1：恢复执行，执行 CAS ====

线程1: head.compare_exchange_strong(local_old, local_next)
  比较：local_old(0x1000) == head(0x1000)?
  当前 head 指向 D，地址恰好也是 0x1000
  比较通过！CAS 成功！head = local_next = 0x2000

  问题1：0x2000 是 B 的地址，B 已被释放！head 指向已释放内存 → 野指针
  问题2：从 head 出发找不到 D 和 C，它们从链表中丢失 → 内存泄漏
```

**问题拆解：**

CAS 把 `head` 从 D 改成了 B(0x2000)，这本质上是一次"pop D"操作（因为 D 恰好占用了 A 的地址，CAS 以为 pop 的是 A）。但 B 已经被释放，所以产生了两个后果：

```
CAS 之前:
  head → D(0x1000) → C → null     ← D 和 C 都可达

CAS 之后: head = B(0x2000)
  head → B(0x2000) → ???            ← B 是已释放内存，野指针
         D(0x1000) → C → null      ← D 不可达，C 也不可达，内存泄漏
```

问题1 和问题2 其实是同一个根源：CAS 把 head 切换到了已释放的 B，导致一条链断了（野指针），另一条链丢了（内存泄漏）。

**问题的本质：** CAS 只比对了指针的地址值 0x1000，
不知道这个地址上住的已经从"A节点"变成了"D节点"。
线程1以为"没人动过head"，实际上中间已经 pop 了两个节点、push 了一个新节点。
CAS 成功不是因为链表没变，而是因为 A 的地址碰巧被复用了。

### 11.2 解决方案

#### 方案1：带版本号（Tagged Pointer）

每次修改 head 时版本号+1，CAS 同时比对指针和版本号。即使地址被复用，版本号也不同了。

```cpp
struct TaggedPointer {
    Node* ptr;
    uint64_t version;
};

std::atomic<TaggedPointer> head{TaggedPointer{nullptr, 0}};

// push
void push(const T& value) {
    Node* new_node = new Node{value, nullptr};
    TaggedPointer old_head = head.load();
    do {
        new_node->next = old_head.ptr;
    } while (!head.compare_exchange_weak(
        old_head, TaggedPointer{new_node, old_head.version + 1}));
}

// pop
bool pop(T& result) {
    TaggedPointer old_head = head.load();
    do {
        if (old_head.ptr == nullptr) return false;
    } while (!head.compare_exchange_weak(
        old_head, TaggedPointer{old_head.ptr->next, old_head.version + 1}));
    result = std::move(old_head.ptr->data);
    delete old_head.ptr;
    return true;
}
```

用上面的 ABA 场景验证：

```
线程1: old_head = {ptr=0x1000, version=1}
线程2: pop A, pop B, push D → head = {ptr=0x1000, version=4}
线程1: CAS({ptr=0x1000, version=1}, {ptr=0x2000, version=2})
  比较：version=1 != head.version=4 → CAS 失败！
  线程1 重试，拿到新的 head，不会出错
```

注意：`compare_exchange_weak` 需要同时比较 ptr 和 version 两个字段，需要硬件支持双字 CAS（128 位 on 64-bit 系统）。

#### 方案2：Hazard Pointer（危险指针）

pop 之前先"声明"我要访问的节点，其他线程延迟释放该节点，直到没有人在用。

```
线程1: hazard_ptr = A         // 声明：我正在用 A
线程2: pop A，但 A 被线程1 声明了，不能立即 delete
        把 A 放入"待回收列表"
线程1: CAS 执行时，head 已经不是 A → CAS 失败，重试
        重试前获得新的 head = D
        hazard_ptr = nullptr    // 释放声明
线程2: 检查待回收列表，A 已经没人声明了 → 安全 delete A
```

核心思路：**不阻止 ABA 发生，而是延迟释放**。节点不会被复用，地址就不会巧合相同，ABA 自然消失。

```cpp
// 简化的 hazard pointer 思路
std::atomic<Node*> hazard_ptr;       // 声明"我正在用这个节点"
std::vector<Node*> retire_list;      // 待回收列表

bool pop(T& result) {
    Node* old_head;
    do {
        old_head = head.load();
        hazard_ptr.store(old_head);      // 步骤1：声明我要用 old_head
        if (old_head != head.load()) {   // 步骤2：双重检查，防止 head 已变
            hazard_ptr.store(nullptr);
            continue;
        }
        if (old_head == nullptr) {
            hazard_ptr.store(nullptr);
            return false;
        }
    } while (!head.compare_exchange_weak(old_head, old_head->next));
    
    hazard_ptr.store(nullptr);           // 步骤3：释放声明
    result = std::move(old_head->data);
    // 不能立即 delete old_head，要检查是否有人声明了它
    if (!is_hazard(old_head)) {
        delete old_head;
    } else {
        retire_list.push_back(old_head);  // 延迟释放
    }
    return true;
}
```

#### 方案3：引用计数（引用计数法）

类似 hazard pointer，但用原子计数代替显式声明。每个节点带一个 `ref_count`，pop 时+1，用完后-1，只有 ref_count 归零时才真正释放。

```cpp
struct Node {
    T data;
    Node* next;
    std::atomic<int> ref_count{0};
};

bool pop(T& result) {
    Node* old_head = head.load();
    do {
        if (old_head == nullptr) return false;
    } while (!head.compare_exchange_weak(old_head, old_head->next));
    
    // old_head 引用计数 > 0，其他线程可能还在用
    // 等引用计数归零才能 delete
    if (old_head->ref_count.fetch_sub(1) == 1) {
        delete old_head;
    }
    result = std::move(old_head->data);
    return true;
}
```

#### 三种方案对比

| 方案 | 原理 | 优点 | 缺点 |
|------|------|------|------|
| 版本号 | CAS 同时比指针和版本号 | 简单直接 | 需要硬件支持双字 CAS |
| Hazard Pointer | 延迟释放被声明的节点 | 不依赖特殊硬件 | 实现复杂，每个线程需维护声明列表 |
| 引用计数 | 计数归零才释放 | 实现相对简单 | 计数本身也需要原子操作，性能开销 |

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

// Full屏障：之前的写不能重排到之后，之后的读也不能重排到之前
// 等价于 Release + Acquire，且额外禁止 StoreLoad 重排
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

### 14.1 不用锁，怎么防并发冲突？

无锁的核心思路就三种：**原子指令、避免共享、CAS循环**。

**1. 原子指令——一个数，大家抢着改**

比如引用计数：10个线程同时 `nref++`，不用锁，直接 `fetch_add(1)`。CPU 在指令级别保证不会丢计数，硬件做了这个活。

**2. 避免共享——我不跟你抢，你就不用锁**

每个线程只改自己的数据，最后再合并。比如10个线程各数各的，最后把10个计数加起来。本质是把"同时改一个数"变成"各自改各的，最后汇总"。Ceph 的 Crimson 就是这个思路：每个核有自己的内存区域，核之间只传消息，不共享数据，自然不需要锁。

**3. CAS循环——先看一眼，没变就改，变了就重来**

比如要往链表头部插入节点：先拍个照"当前 head 是 A"，然后试图把 head 从 A 改成新节点。如果其他线程已经把 head 改成了 B，我的 CAS 就失败，那就拿新值重来。和抢票一样：看到还有票→下单→发现被别人抢了→刷新页面重来。

但这三种方法各有局限：原子指令只能改一个数；避免共享需要算法配合；CAS 循环会遇到 ABA 问题（10.1 节），还要小心内存序（8.3 节）。

### 14.2 何时使用无锁

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