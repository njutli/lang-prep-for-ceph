# 阶段六：Boost库（按需学习）

## 0. Boost 是什么？为什么 Ceph 离不开它？

### 1.1 历史与定位
Boost 诞生于 1998 年，由多位 C++ 标准委员会成员共同创建。它的初衷是作为一个**“C++ 标准库的实验场”**。
*   **实验场模式**：新特性先在 Boost 中成熟，经过多年实战验证后，再被采纳为 C++ 标准。例如 `shared_ptr`、`asio`、`variant`、`optional` 最初都来自 Boost，后来才进入 C++11/17 标准。
*   **许可证**：Boost 使用 **BSL (Boost Software License)**，极其宽松，允许商业软件免费链接和使用，这也是 Ceph 能大量集成它的原因。

### 1.2 与 Ceph 的关系
Ceph 是一个诞生于 2000 年代中期的分布式系统。在 C++11/14 标准普及之前，C++98/03 的标准库非常贫乏（没有智能指针、没有异步 IO、没有可选值）。**Ceph 深度绑定了 Boost 来填补这些底层基础设施的空白。**

即使现代 C++ 已经标准化了很多特性，Ceph 的旧代码中依然保留了大量 Boost 用法（如 `boost::asio` 网络层、`boost::program_options` 配置解析、`boost::container::small_vector` 性能优化等）。

> **核心建议**：不要试图通读 Boost 文档。学习原则是：**“遇到什么查什么，优先学 Ceph 真正用到的，深入看它是如何实现的。”**

---

## 1. Boost.Container (容器优化)

### 1.1 small_vector：栈优先的动态数组

**核心原理与源码分析：**
普通 `std::vector` 无论多小都会分配堆内存。`small_vector` 解决的是**短容器在堆上的性能损耗**。
*   **源码路径**: <br> `/home/i_ingfeng/boost_1_85_0/boost/container/small_vector.hpp`
*   **实现机制**:
    *   核心类 `small_vector_base<T, N>` 内部维护了一个 **联合体 (Union)** 结构。
    *   分支 1：一个大小为 N 的嵌入式数组 `raw storage[N * sizeof(T)]`（用于栈内存）。
    *   分支 2：一个指向堆的指针 `pointer`（超出 N 时使用）。
    *   `push_back` 时，如果 `size == capacity` 且 `size < N`，直接在嵌入式数组中使用 Placement New 构造对象，**零 malloc**。
    *   一旦超过 N，它会分配一块堆内存，将数据从栈搬移过去，之后表现得和普通 vector 一样。

**Ceph 实战场景：**
在 `src/osd/osd_types.h` 的 PG（Placement Group）管理中，PG 通常只有 1~2 个 OSD 副本。如果用 `std::vector`，即使只存 2 个 ID 也会触发 `malloc`。使用 `small_vector<pg_shard_t, 4>` 能让绝大多数情况下的内存分配在栈上瞬间完成。

**用法示例：**
```cpp
#include <boost/container/small_vector.hpp>
// 定义：N=4 的元素在栈上
boost::container::small_vector<int, 4> vec; 
vec.push_back(1); // 栈上操作
// 超过 4 个元素时，底层才自动切换为堆分配
```

---

## 2. Boost.Intrusive (侵入式容器)

### 2.1 intrusive list / pointer

**核心原理与源码分析：**
传统容器（如 `std::list`）是“非侵入式”的，容器节点与数据分离，插入时必须 `new Node()`。`intrusive` 则是“侵入式”的，**要求数据对象自带容器的指针**。
*   **源码路径**: <br> `/home/i_ingfeng/boost_1_85_0/boost/intrusive/list_base_hook.hpp` (Hook定义) <br> `/home/i_ingfeng/boost_1_85_0/boost/intrusive/list.hpp` (容器实现)
*   **实现机制**:
    *   对象必须继承 `list_base_hook<>`（或者包含 `list_member_hook<>` 成员）。
    *   Hook 内部定义了 `next` 和 `prev` 指针。
    *   容器本身只保存一个 Sentinel 节点（Head），不包含数据。
    *   **优势**：`push_back` 不需要分配内存，直接修改传入对象内部的 `next/prev` 指针链接到链表。**零内存碎片，极快的插入速度**。

**代码对比：为什么它比 std::list 更快？**

| 特性 | 传统容器 `std::list<T>` (非侵入式) | Boost.Intrusive `list<T>` (侵入式) |
| :--- | :--- | :--- |
| **操作示意** | `std::list<MyData> l; l.push_back(data);` | `boost::intrusive::list<MyData> l; l.push_back(data);` |
| **内存行为** | 容器内部执行 `new Node()`。 | **无 `new` 操作**。 |
| **数据流向** | 必须把 `data` **拷贝**或移动到容器内部的新节点中。 | 对象本身就是节点，通过对象内部的指针直接串联。 |
| **结果** | 栈上的 `data` 和链表里的节点是**两份**数据。 | 栈上的 `data` **直接**变成了链表的一部分。 |

**图解内存布局差异：**
*   **std::list**:
    `[对象 data] (独立)` ... `[堆节点 Node1] <-> [堆节点 Node2] ...`
*   **Intrusive**:
    `[对象 data (内含 prev/next 指针)] <---> [对象 data (内含 prev/next 指针)]`

**Ceph 实战场景：**
Ceph 的缓存系统（`ObjectCacher`）和引用计数（`RefCountedObj`）。对象需要在多个链表（LRU、脏数据链、IO 队列）中频繁进出。侵入式链表避免了在这些操作中反复 `new/delete` 节点，提升了 IOPS。

**用法示例：**
```cpp
#include <boost/intrusive/list.hpp>
struct MyObj : public boost::intrusive::list_base_hook<> {
    int id;
};
boost::intrusive::list<MyObj> my_list;
MyObj obj; my_list.push_back(obj); // obj 自己进入了链表，无需分配节点
```

---

## 3. Boost.SmartPtr (智能指针)

### 3.1 intrusive_ptr

**前置条件（关键区别）：**
使用 `intrusive_ptr` 有一个强制要求：**你的类必须自己包含引用计数变量**。如果对象本身没有计数，就无法使用此智能指针。
*   **`std::shared_ptr` (非侵入式)**：它会在堆上额外分配一块独立的**控制块 (Control Block)** 来存引用计数。通常伴随 2 次内存分配，且数据和计数在物理上不连续。
*   **`boost::intrusive_ptr` (侵入式)**：它**不分配、也不存储**引用计数。它复用你对象内部已有的空间。

**核心原理与性能优势：**

*   **源码路径**: <br> `/home/i_ingfeng/boost_1_85_0/boost/smart_ptr/intrusive_ptr.hpp`
*   **为何 Ceph 偏爱它 (Zero Allocation & Cache)**：
    1.  **内存碎片**: `shared_ptr` 需要 `new` 控制块。`intrusive_ptr` 只有 1 次分配（对象本身），减少内存碎片。
    2.  **缓存命中率**: `shared_ptr` 的计数在远处的堆地址，更新时易触发 Cache Miss。`intrusive_ptr` 的计数就在对象头部，CPU 加载对象时已将计数带入缓存，更新时直接 **Cache Hit**。

**实现机制：ADL 自动查找**

`intrusive_ptr` 内部只有一个原始指针。它通过 **ADL（参数依赖查找）** 机制，自动调用全局函数：
*   `intrusive_ptr_add_ref(T*)`
*   `intrusive_ptr_release(T*)`

**完整用法演示：**

```cpp
#include <boost/smart_ptr/intrusive_ptr.hpp>

// ==========================================
// 1. 【用户代码】：业务类 (必须内嵌计数!)
// ==========================================
class MyObj {
    mutable int ref_count_ = 0; // 【核心】对象自己维护计数
public:
    void get() const { ++ref_count_; }
    void put() const { if(--ref_count_ == 0) delete this; }
};

// ==========================================
// 2. 【用户代码】：胶水函数
// ==========================================
// 名字必须是规定的，通过 ADL 与上面的类绑定
// (注：建议放在类的命名空间中以避免同名冲突)
void intrusive_ptr_add_ref(MyObj* p) { p->get(); }
void intrusive_ptr_release(MyObj* p) { p->put(); }

// ==========================================
// 3. 【Boost 效果】
// ==========================================
void demo() {
    // 构造：调用 add_ref -> 计数 0->1 (无额外 new)
    boost::intrusive_ptr<MyObj> ptr(new MyObj()); 

    // 拷贝：调用 add_ref -> 计数 1->2
    boost::intrusive_ptr<MyObj> copy = ptr; 
} // 离开作用域 -> 调用 release -> 计数归零 -> delete
```

**Ceph 源码对照：**
Ceph 的 `src/common/RefCountedObj.h` 就是这种思想的直接体现。其内部维护 `atomic<uint64_t> nref`，配合侵入式指针管理 Buffer/Message 生命周期，以极低开销处理高并发引用计数操作。

---

## 4. Boost.Asio (异步 IO)

### 4.1 核心概念：什么是 `io_context::run()`？

用一句话概括：**`run()` 就是"打工者"，而所有 `async_` 调用就是"派活儿"**。

| 概念 | 说明 |
|---|---|
| `io_context` | 任务分发中心，内部维护一个完成队列 |
| `async_connect` / `async_read` / `async_write` / `async_accept` | **派活儿**：告诉 OS 去关注某个 IO 事件，事件完成后把回调塞进队列。调用完 **立刻返回**，不阻塞 |
| `io_context.run()` | **打工**：在一个循环里，不断检查是否有事件完成，有就执行回调；没有就阻塞等待（底层挂起线程，不消耗 CPU） |

**关键理解：`async_` 调用和 `run()` 是解耦的**

```
线程 A（主线程 / 业务线程）          线程 B（io_context.run() 所在的 IO 线程）
========================              =======================================
async_connect(...)  ← 提交任务
async_read(buf)     ← 再提交一个
async_write(data)   ← 再提交一个
"好了，我的活派完了"
去做别的事了 ────────────────────►    run() 循环 ← 一直在这等着
                                        ↓
                                 连接成功 → 执行 connect 回调
                                        ↓
                                 数据到达 → 执行 read 回调
                                        ↓
                                 写入完成 → 执行 write 回调
```

这就是 Boost Asio 的 **Proactor** 模式：
1. **提交** IO 请求（`async_`）→ 立刻返回
2. 线程可以去干别的
3. IO 完成了 → 回调被 `run()` 执行

### 4.2 源码路径
* `/home/i_ingfeng/boost_1_85_0/boost/asio/io_context.hpp` (调度核心)
* `/home/i_ingfeng/boost_1_85_0/boost/asio/detail/impl/epoll_reactor.hpp` (Linux epoll 封装)

### 4.3 Ceph 实战场景

Ceph 的 `src/msg/async/AsyncMessenger.cc` 网络层是整个集群通信的命脉。其核心结构：

```
┌───────────────┐   connect/write/read   ┌────────────────────┐
│  OSD / MON    │ ◄──────────────────► │  io_context.run()   │
│  (业务线程)   │                        │  (NetworkWorker 线程)│
├───────────────┤                       ├────────────────────┤
│ submit_message│   ← 派活儿 async_     │ epoll_wait() 等待  │
│ handle_msg    │   ← 回调中执行         │ 调用 callback      │
└───────────────┘                       └────────────────────┘
```

* 每个 Ceph daemon（OSD/MON/MDS/Client）都有自己的 `EntityMessenger`
* 内部维护一个或多个 `io_context`，由 dedicated 线程调用 `run()`
* 多核场景下：多个线程 **同时 `run()`** 同一个 `io_context`，自动负载均衡

### 4.4 完整示例：Server + Client 两进程通信

> 完整代码见 `exercises/asio_server.cpp` 和 `exercises/asio_client.cpp`

#### 编译

```bash
g++ -std=c++17 -pthread exercises/asio_server.cpp -lboost_system -o exercises/asio_server
g++ -std=c++17 -pthread exercises/asio_client.cpp -lboost_system -o exercises/asio_client
```

#### 运行（两个终端）

```
终端 1:  ./asio_server          # 启动服务端，监听 9876 端口
终端 2:  ./asio_client          # 连接服务端，发送 "Hello from client!"
```

服务端会 echo 回来，两端都打印日志。

#### Server 关键代码节选

```cpp
class Server {
    tcp::acceptor acceptor_;

public:
    Server(io_context& ctx, unsigned short port)
        : acceptor_(ctx, tcp::endpoint(tcp::v4(), port))
    {
        do_accept();  // 启动第一个异步 accept
    }

private:
    void do_accept() {
        // async_accept: 提交 accept 请求，立刻返回
        acceptor_.async_accept(
            [this](error_code ec, tcp::socket socket) {
                if (!ec) {
                    // 新连接来了，创建 Session 处理
                    std::make_shared<Session>(std::move(socket))->start();
                }
                // 递归调用，持续接受新连接
                do_accept();
            });
    }
};
```

#### Client 关键代码节选（回调链展示）

```cpp
// 1) async_connect →
socket.async_connect(ep,
    [&](error_code ec) {
        if (ec) return;
        // 2) 连接成功 → async_write →
        async_write(socket, buffer(msg),
            [&](error_code ec2, size_t len) {
                if (ec2) return;
                // 3) 发送完成 → async_read →
                socket.async_read_some(buffer(buf),
                    [&](error_code ec3, size_t len) {
                        // 4) 收到 server 回复，处理
                    });
            });
    });

// async_connect 提交后，到这里时连接还没建立
cout << "派活儿结束，等待 ctx.run() 调度回调" << endl;
```

#### 日志解读

**Server 端（关键行）**:
```
[Server] 开始监听 0.0.0.0:9876
[io_context] run() 启动，进入事件循环...
[Main]   io_context.run() 已交到后台线程
[Main]   主线程现在是空闲的，可以...      ← 证明 async_accept 不阻塞
[Session] 新连接建立，来自 127.0.0.1:xxxxx
[Session] 读到 28 字节: "Hello from client! 你好，Ceph!"
[Session] 发送完成，继续等下一条...
```

**Client 端（关键行）**:
```
[Client] async_connect 已提交，线程自由了   ← 连接还未建立，代码已经往下走
[Client] async_write 已提交，线程自由了    ← 数据还没发，代码又往下走
[Client] ctx.run() 返回，程序结束
[Client] 收到回复: "Hello from client! 你好，Ceph!"   ← 回调中执行
```

**看到 "线程自由了" 和 "主线程现在是空闲的" 这些日志，就说明 `async_` 方法调用完就返回了——线程不需要等待 IO 完成，这就是"异步"的本质。**

---

## 5. 其他常用 Boost 组件

### 5.1 boost::format (格式化字符串)
*   **源码路径**: `boost/format.hpp`
*   **原理**: 利用模板递归解析 `%` 操作符传入的参数，存入内部 `basic_format<T>` 类的 vector 中，最后调用 `str()` 进行类型安全替换。
*   **Ceph 现状**: Ceph 早期大量使用它记录日志。现代 Ceph 已迁移到 `fmt::format`（因为 `boost::format` 编译慢、运行时开销大）。

### 5.2 boost::program_options (命令行解析)
*   **源码路径**: `boost/program_options/options_description.hpp`
*   **原理**: 定义了 `option_description`，通过 `store(parse_command_line...)` 将 argv 解析为 key-value 对（`variables_map`）。

---

## 学习建议

1.  **不要全学**：Boost 有超 100 个子库，90% 与 Ceph 无关。
2.  **抓大放小**：重点理解 **Intrusive 思想**、**Small_vector 优化**、**Asio 异步模型**。
3.  **拥抱现代 C++**：遇到 `boost::optional/variant`，直接映射到 `std::` 即可。
