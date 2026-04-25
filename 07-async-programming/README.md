# 阶段七：异步编程（Seastar框架与 Ceph 架构）

## 🛑 初学者必读：现在需要学 Seastar 吗？

**结论：初步学习阶段，完全不需要。**

在阅读 Ceph 源码的初期，你应该**跳过** Seastar，直接将精力集中在**经典架构 (Classic OSD)** 上。

| 架构 | 所在目录 | 技术栈 | 推荐 |
| :--- | :--- | :--- | :--- |
| **经典 OSD (Mainline)** | `src/osd/` | `std::thread` + `Boost.Asio` + `Coroutines` (C++20) | ✅ **重点学习** (生产环境主流) |
| **Crimson (新一代)** | `src/crimson/` | **Seastar 框架** (Reactor 模型) | ⏳ 后期/高级选读 |

**为什么不用急着学 Seastar？**
1.  **业务逻辑相通**：无论是经典版还是 Crimson，Ceph 核心的 CRUSH 算法、PG 状态机、数据恢复流程是通用的。
2.  **难度陡峭**：Seastar 使用了大量的模板元编程、无锁数据结构和特殊的内存管理。初学者容易迷失在框架实现中，反而忽略了 OSD 业务代码本身的逻辑。
3.  **Ceph 还在过渡中**：虽然 Crimson 是未来，但绝大多数现网集群依然运行的是经典 OSD。

> **建议**：只要掌握了前一章的 **Boost.Asio 异步模型**（回调、Proactor 模式）和 **多线程并发基础**，你就足以理解 Ceph 网络层和大多数业务逻辑了。

---

Ceph 的架构师正在尝试将 **Seastar** 引入 Ceph，以利用其单核极致性能。理解它有助于你理解 Ceph 在网络和存储 IO 上的优化方向。本章重点结合代码实例，解释 Seastar 与传统异步编程的区别。

## 1. Seastar vs. 传统并发模型

Seastar 不仅仅是一个库，它是一种全新的**并发架构**。

| 特性 | 传统模型 (POSIX Threads) | Seastar 模型 (Crimson) |
| :--- | :--- | :--- |
| **架构** | **Thread-per-Connection**: 每个请求一个线程，共享内存。 | **Reactor-per-Core**: 每个 CPU 核心一个独立线程，**内存隔离** (Shared-Nothing)。 |
| **竞争** | **高锁竞争**: 必须使用 `std::mutex` 保护共享数据。 | **无锁 (Lock-free)**: 每个核心的数据互不干扰，**不需要 Mutex**。 |
| **通信** | 全局共享内存。 | 核间通信通过**队列** (Queue) 传递消息，避免数据竞争。 |
| **IO 模式** | 阻塞调用 (`read/write`) 依赖系统调用。 | 异步轮询 (`polling`) + 异步回调 (`.then()`)，极致利用硬件带宽。 |

---

## 2. 核心代码模式分析

请参考子目录下的完整源码：`exercises/seastar_examples.cpp`。即使无法编译，阅读代码也能帮助你理解其编程范式。

### 2.1 Reactor 循环 (The Event Loop)

在传统程序中，`main` 函数线性执行。而在 Seastar 中，程序的入口 `main` 只是用来配置环境，真正的核心是 **App.run** 启动的事件循环。

```cpp
// 对应 exercises/seastar_examples.cpp -> main()
int main(int argc, char** argv) {
    app_template app;
    // run() 会接管当前线程，进入死循环 (Event Loop)，直到所有任务完成
    return app.run(argc, argv, &run_app); 
}
```
*   **为什么不能阻塞？**
    由于每个核心只有一个循环，如果你在回调中调用阻塞函数（如 `std::this_thread::sleep` 或读取大文件），整个 CPU 核心上的所有请求都会停摆（Starvation）。

### 2.2 Future 链 (Pipeline)

这是 Seastar 业务逻辑的写法。因为不能阻塞，所以必须通过返回 `future<>` 并在其上注册 `.then()` 来表达逻辑的先后关系。

*   **代码分析**: `exercises/seastar_examples.cpp#demo_chain`
    *   `read_object_from_disk` 立即返回一个未完成的 future。
    *   随后 `.then()` 注册了回调函数（Continuation）。当磁盘 IO 完成后，Seastar Scheduler 会自动在**同一个 Reactor 线程**中执行 `.then` 中的闭包。
    *   **对比**: 传统 C++11 `std::async` 通常返回一个 `std::future`，然后你需要调用 `.get()` (这将阻塞) 来获取结果；而 Seastar 永远**不要调用 .get()**。

### 2.3 Sharded (分布式/分片服务)

这是阅读 Ceph Crimson 源码的关键。在 Crimson 中，PG 和 OSD 往往不是单个对象，而是分布在各个 CPU 核心上的。

*   **代码分析**: `exercises/seastar_examples.cpp#demo_sharding`
    *   `sharded<PerfCounter>` 是 Seastar 提供的分片容器。
    *   当你调用 `counters.start()` 时，Seastar 会在每个 CPU 核上都实例化一个 `PerfCounter`。
    *   **invoke_on_all**: 这是一个广播操作。它会向所有核发送消息，让它们各自执行传入的 lambda。
    *   **意义**: 通过分片，原本需要锁保护的 `PerfCounter.count` 变成了每个核的局部变量，彻底消除了锁竞争。

### 2.4 Semaphore (流量控制)

因为不能用 Mutex，Seastar 提供了非阻塞的 **信号量 (Semaphore)**。

*   **代码分析**: `exercises/seastar_examples.cpp#demo_semaphore`
    *   当资源不足时，`semaphore::wait` 返回一个 pending 的 future，任务挂起，线程去处理其他请求。
    *   这与操作系统的信号量不同，它纯粹是**用户态**的调度机制，没有上下文切换的开销。

---

## 3. 总结

在（未来的）阅读 `src/crimson` 源码时，你需要完成思维转换：
1.  看到 `future<T>`，意识到这是一个**承诺**，会在未来的某个时刻完成，而不是当前的计算结果。
2.  看到 `seastar::sharded`，意识到这是一个**多核并行**的实体，而不是单例。
3.  看到 `co_await` (C++20)，意识到它是 `.then()` 的语法糖，虽然写法像同步，但底层依然是异步非阻塞的。
