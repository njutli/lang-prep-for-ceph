# 并发编程练习索引

## 练习列表

| 练习 | 文件 | 难度 | 覆盖知识点 |
|------|------|------|------------|
| 1 | atomic_counter.cpp | ★☆☆ | 原子操作、内存序 |
| 2 | producer_consumer.cpp | ★★☆ | 条件变量、互斥量 |
| 3 | thread_pool.cpp | ★★☆ | 线程池、任务队列 |
| 4 | lock_free_stack.cpp | ★★★ | 无锁编程、CAS、ABA |
| 5 | mpsc_queue.cpp | ★★★ | 无锁队列、哨兵节点、exchange |

## 编译命令

```bash
# 基础编译
g++ -std=c++17 -pthread -O2 atomic_counter.cpp -o atomic_counter
g++ -std=c++17 -pthread -O2 producer_consumer.cpp -o producer_consumer
g++ -std=c++17 -pthread -O2 thread_pool.cpp -o thread_pool
g++ -std=c++17 -pthread -O2 lock_free_stack.cpp -o lock_free_stack
g++ -std=c++17 -pthread -O2 mpsc_queue.cpp -o mpsc_queue

# 使用ThreadSanitizer检测数据竞争
g++ -std=c++17 -pthread -fsanitize=thread -g atomic_counter.cpp -o atomic_counter_tsan
g++ -std=c++17 -pthread -fsanitize=thread -g lock_free_stack.cpp -o lock_free_stack_tsan
g++ -std=c++17 -pthread -fsanitize=thread -g mpsc_queue.cpp -o mpsc_queue_tsan
```

## 学习顺序

1. **atomic_counter.cpp** - 理解原子操作和内存序
2. **producer_consumer.cpp** - 理解条件变量和锁的使用
3. **thread_pool.cpp** - 理解线程池模式
4. **lock_free_stack.cpp** - 进阶：理解无锁编程和CAS
5. **mpsc_queue.cpp** - 进阶：理解无锁队列、哨兵节点、exchange操作

## 关键概念对照

| 练习 | 概念 |
|------|------|
| atomic_counter | `std::atomic`, `fetch_add`, `memory_order` |
| producer_consumer | `std::mutex`, `std::condition_variable`, `std::unique_lock` |
| thread_pool | `std::thread`, `std::function`, `std::future` |
| lock_free_stack | `compare_exchange_weak`, `memory_order_acquire/release`, CAS循环 |
| mpsc_queue | `atomic::exchange`, 哨兵节点, MPSC队列, `memory_order_acq_rel` |

## Ceph代码对照

| 练习 | Ceph源码 |
|------|----------|
| atomic_counter | `src/common/RefCountedObj.h` - 引用计数 |
| producer_consumer | `src/common/Finisher.h` - 完成队列 |
| thread_pool | `src/osd/OpWQ.h` - 操作工作队列 |
| lock_free_stack | `src/crimson/` - 无锁数据结构 |
| mpsc_queue | `src/common/Finisher.h` - 异步任务队列（MPSC场景） |