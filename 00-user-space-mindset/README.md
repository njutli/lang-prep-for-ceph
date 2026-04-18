# 阶段零：用户态工程思维（关键概念）

> 这是读Ceph的**最重要基础**。没有这个意识，你可能看懂每个函数，但串不起来整个请求流。

## 为什么需要这个阶段？

你有内核开发背景，擅长跟踪同步调用链。但Ceph是**用户态分布式系统**，风格完全不同：

### 内核模式 vs 用户态分布式模式

| 内核模式 | 用户态分布式模式 |
|---------|----------------|
| 函数直接调用 | 提交任务 → 入队 → 异步完成 |
| 调用链清晰 | 回调续跑、状态机推进 |
| 同步等待 | 异步非阻塞 |
| 单一执行流 | 跨多个线程/模块/节点 |
| 锁保护临界区 | 消息传递、状态机 |

## 1. 异步编程模型

### 1.1 异步是什么？

```cpp
// 同步模式（内核常见）
int read_data() {
    request_to_disk();      // 阻塞等待
    return data;            // 返回时数据已就绪
}

// 异步模式（Ceph常见）
void read_data() {
    submit_request(callback);  // 立即返回，不等待
    // 数据到达时，callback被调用
}
```

**关键意识转换：**
- 不再是"调用 → 等待 → 返回"
- 而是"提交 → 返回 → 完成时回调"

### 1.2 回调模式

```cpp
// Ceph中的回调基类（src/include/Context.h）
class Context {
protected:
    virtual void finish(int r) = 0;  // 子类实现完成逻辑
public:
    virtual void complete(int r) {
        finish(r);   // 调用子类实现
        delete this;  // 注意：自动释放自己！
    }
};

// 使用示例（Ceph中到处可见）
class MyCallback : public Context {
    void finish(int r) override {
        // 操作完成后被执行
    }
};

store->write_transaction(t, new MyCallback);
// 函数返回，但写入还在进行...完成后自动调用 MyCallback::finish(0)
```

**注意几个 Ceph 回调的特点：**
- 基类是 `Context`，不是 `Callback`
- `complete()` 调用 `finish()` 后会 `delete this`——所以回调对象通常用 `new` 创建，**不需要手动释放**
- 传入的是错误码 `int r`，0 表示成功，负数表示错误

### 1.3 为什么Ceph大量用异步？

- **网络延迟**：不能阻塞等网络包
- **并发处理**：一个线程服务多个请求
- **资源效率**：不用为每个请求开一个线程

## 2. 状态机思维

### 2.1 什么是状态机？

请求不是一次走完，而是每次事件推进一个状态：

```
状态图示例：OSD写请求

  [初始] 
     ↓ 收到请求
  [等待认证]
     ↓ 认证通过
  [等待PG准备]
     ↓ PG就绪
  [等待磁盘写入]
     ↓ 写入完成
  [回复客户端]
     ↓
  [结束]
```

### 2.2 Ceph中的真实例子

Ceph 的 PG 状态机有**两层机制**，初学者容易混淆：

#### 第一层：位掩码状态标记（`uint64_t state`）

PG 的状态不是 `enum`，而是**位掩码**——一个 PG 可以**同时**处于多个状态（如 ACTIVE + DEGRADED）：

```cpp
// src/osd/osd_types.h —— 状态定义是宏，不是 enum
#define PG_STATE_CREATING           (1ULL << 0)  // 正在创建
#define PG_STATE_ACTIVE             (1ULL << 1)  // 活跃可服务
#define PG_STATE_CLEAN              (1ULL << 2)  // 干净
#define PG_STATE_PEERING            (1ULL << 12) // 正在建连
#define PG_STATE_DEGRADED           (1ULL << 10) // 降级
#define PG_STATE_RECOVERING         (1ULL << 14) // 正在恢复
// ... 共 30+ 个状态位

// src/osd/PeeringState.h —— 用位运算判断和设置状态
class PeeringState {
    uint64_t state = 0;  // 注意：是 uint64_t，不是 enum
    bool state_test(uint64_t m) const { return (state & m) != 0; }
    void state_set(uint64_t m)   { state |= m; }   // 设置状态位
    void state_clear(uint64_t m) { state &= ~m; }   // 清除状态位

    bool is_active()     const { return state_test(PG_STATE_ACTIVE); }
    bool is_peering()    const { return state_test(PG_STATE_PEERING); }
    bool is_degraded()   const { return state_test(PG_STATE_DEGRADED); }
    bool is_recovering() const { return state_test(PG_STATE_RECOVERING); }
};
```

关键：**state |= m** 意味着状态可以叠加！一个 PG 可以同时是 ACTIVE + DEGRADED + RECOVERING。

#### 第二层：Boost.Statechart 层级状态机（真正驱动状态转换）

状态**转换**不是 switch-case，而是用 **Boost.Statechart** 库实现的层级状态机：

```
层级状态机结构（src/osd/PeeringState.h）：

PeeringMachine（状态机顶层）
  └── Initial → Reset → Started
                         ├── Primary
                         │    └── Peering
                         │         ├── GetInfo → GetLog → GetMissing → WaitUpThru
                         │         └── Down / Incomplete
                         │    └── Active
                         │         ├── Activating
                         │         ├── Recovered
                         │         ├── Clean
                         │         └── Recovering / Backfilling / ...
                         └── Stray
                              └── ReplicaActive

每个状态是一个 C++ struct，继承 boost::statechart::state：
  struct Active : boost::statechart::state< Active, Primary, Activating >, NamedState {
      // 收到 AdvMap 事件时的回调
      boost::statechart::result react(const AdvMap&) { ... }
      // 收到 ActMap 事件时的回调
      boost::statechart::result react(const ActMap&) { ... }
  };

状态转换通过事件驱动：
  boost::statechart::transition< Activate, Active >  // 收到 Activate 事件 → 转到 Active 状态
```

#### 两层如何协作？

```
                     事件到达
                        ↓
          PeeringMachine 收到事件
                        ↓
          当前 State 的 react() 处理
           ┌──────────┴──────────┐
           ↓                     ↓
     切换到新 State           设置/清除状态位
  (Boost.Statechart)       (state_set / state_clear)
                                ↓
                        更新 uint64_t state
```

源码中的典型转换（`src/osd/PeeringState.cc`）：
```cpp
// 进入 Peering 状态：设 PEERING 位
void PeeringState::Peering::enter() {
    ps->state_set(PG_STATE_PEERING);
}

// Peering 完成，进入 Active：清 PEERING，设 ACTIVE
void PeeringState::Active::enter() {
    ps->state_clear(PG_STATE_PEERING);
    ps->state_set(PG_STATE_ACTIVE);
}

// 数据不完整：设 DEGRADED 位（注意：与 ACTIVE 不互斥！）
if (degraded) {
    ps->state_set(PG_STATE_DEGRADED);
} else {
    ps->state_clear(PG_STATE_DEGRADED);
}
```

**读代码时要养成习惯：**
1. 这个请求当前在什么状态？（看 `uint64_t state` 的各个位）
2. 什么事件会推进状态？（看 `react()` 函数和 `transition` 定义）
3. 状态推进后会做什么？（看各 State 的 `enter()` 函数）

## 3. 线程模型

### 3.1 Ceph线程模型概览

```
┌─────────────────────────────────────────────────────────┐
│                     OSD进程                               │
├─────────────────────────────────────────────────────────┤
│  ┌───────────────┐  ┌──────────────┐  ┌──────────────┐│
│  │ 网络线程      │  │ 工作线程池  │  │ 磁盘IO线程    ││
│  │ Messenger     │  │ ShardedOpWQ │  │ BlueStore     ││
│  │(ms_fast_      │  │ + OpScheduler│  │ ObjectStore   ││
│  │ dispatch)     │  │              │  │               ││
│  └───────┬───────┘  └──────┬───────┘  └──────┬───────┘│
│          │                 │                  │        │
│          └─────────────────┼──────────────────┘        │
│                            ↓                            │
│  注：网络线程通过 ms_fast_dispatch() 直接入队到           │
│  ShardedOpWQ，不经中间消息队列。ShardedOpWQ 按 PG 分片。  │
└─────────────────────────────────────────────────────────┘
```

### 3.2 读代码时要问的问题

**看到任何函数，要问：**
1. 这个函数在哪个线程被调用？
2. 它操作的数据被谁共享？
3. 是否需要锁？
4. 它可能阻塞吗？

**看到任何数据结构，要问：**
1. 谁创建？谁释放？
2. 哪些线程访问？
3. 有没有并发问题？

## 4. 消息传递

### 4.1 Ceph的消息模型

```cpp
// OSD 继承 Dispatcher，Messenger 通过两个接口分发消息：

// 1. ms_fast_dispatch — 不加锁，快速路径（src/osd/OSD.cc）
void OSD::ms_fast_dispatch(Message *m) {
    switch (m->get_type()) {
    case MSG_OSD_PG_NOTIFY:
        // peering 消息，直接 enqueue_peering_evt
        handle_fast_pg_notify(static_cast<MOSDPGNotify*>(m));
        return;
    // ... 其他 peering 消息也走 fast path
    }
    // 客户端请求：创建 OpRequest 入队到 ShardedOpWQ
    OpRequestRef op = op_tracker.create_request<OpRequest, Message*>(m);
    enqueue_op(spg, std::move(op), epoch);
}

// 2. ms_dispatch — 加 osd_lock，慢路径
bool OSD::ms_dispatch(Message *m) {
    osd_lock.lock();
    _dispatch(m);  // 处理非 fast path 消息
    osd_lock.unlock();
    return true;
}

// 工作线程从 ShardedOpWQ 取任务处理
void ShardedOpWQ::_process(uint32_t thread_index, uint32_t shard_index, ...) {
    OpSchedulerItem item = ...;  // 从调度器取出
    dequeue_op(pg, std::move(op), handle);  // 执行操作
}
```

### 4.2 与直接调用的区别

```cpp
// 直接调用（同步）
void process_request() {
    authenticate();      // 等待完成
    validate();          // 等待完成  
    execute();           // 等待完成
    reply();             // 等待完成
}   // 返回时全部完成

// 消息传递（异步）—— Ceph OSD 的真实模式
void OSD::ms_fast_dispatch(Message *m) {
    switch (m->get_type()) {
    // 客户端写请求 → 入队处理，不等待磁盘
    case CEPH_MSG_OSD_OP:
        enqueue_op(spg, std::move(op), epoch);
        break;
    // PG peering 消息 → 直接入 peering 队列
    case MSG_OSD_PG_NOTIFY:
        enqueue_peering_evt(pgid, pm->get_event());
        break;
    // 磁盘写完成后 → Context 回调触发后续逻辑
    // （不是在同一个函数里继续，而是等事件驱动）
    }
}
```

## 5. 实践建议

### 5.1 建立"请求追踪图"

读到Ceph代码时，画图：

```
请求流程图：

客户端 --[write请求]--> Messenger网络线程
                              |
                         ms_fast_dispatch()
                              |
                         enqueue_op(spg, op, epoch)
                              |
                         ShardedOpWQ（按PG分片调度）
                              |
                         工作线程 dequeue_op()
                              |
                         PrimaryLogPG::do_op()  →  处理请求
                              |
                         ObjectStore::queue_transaction(t, oncommit)
                              |
                         BlueStore 异步写磁盘
                              |
                         写完成 → Context::complete() → 回复客户端
```

### 5.2 建立"对象关系图"

```
对象关系图（基于 src/osd/OSD.h, PG.h）：

OSDService（全局共享服务）
    ├── ObjectStore *store      (存储引擎，通常是 BlueStore)
    ├── Messenger *cluster_messenger  (集群内部网络)
    ├── Messenger *client_messenger   (客户端网络)
    ├── OSDMapRef osdmap       (当前集群状态)
    └── ShardedOpWQ            (操作调度队列)
        
PG（每个 PG 一个实例）
    ├── PeeringState recovery_state  (状态机，核心！)
    ├── const PGPool &pool     (引用，属于 recovery_state)
    ├── const pg_info_t &info  (引用，属于 recovery_state)
    ├── OSDService *osd         (引用，回到全局服务)
    ├── ObjectStore::CollectionHandle ch  (存储引擎句柄)
    └── PGBackend               (后端：ReplicatedBackend 或 ECBackend)
```

### 5.3 建立"线程分布图"

```
线程分布：

Messenger 网络线程（多个）
    └── 收到消息，通过 ms_fast_dispatch() 入队 ShardedOpWQ

ShardedOpWQ 工作线程（多个，按 shard 分片）
    └── 从 OpScheduler 取 PG 级操作，调 dequeue_op() 处理

BlueStore 线程（aio/io_uring 完成线程）
    └── 磁盘 IO 完成后，触发 Context 回调
```

## 6. 检查清单

完成本阶段后，你应该能：

- [ ] 理解同步与异步的区别
- [ ] 理解释为什么Ceph用异步模型
- [ ] 知道什么是回调
- [ ] 知道什么是状态机
- [ ] 理解 Ceph PG 状态机的两层：位掩码状态位 + Boost.Statechart 层级状态机
- [ ] 能说出"请求来了 → 入队 → 异步处理 → 回调"的流程
- [ ] 读代码时会问"这个函数在哪个线程执行？"

## 下一步

这些概念理解后，再学C++语法，效率会高很多。

因为你知道：
- 为什么要学类和对象 → Ceph用对象组织代码
- 为什么要学虚函数 → Ceph用接口+实现分离
- 为什么要学回调 → Ceph到处都是异步完成
- 为什么要学线程 → Ceph多线程处理请求