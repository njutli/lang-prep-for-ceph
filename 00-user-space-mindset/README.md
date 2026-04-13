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
// Ceph中常见的回调模式
class CompletionCallback {
public:
    virtual void complete(int result) = 0;
};

void async_operation(CompletionCallback* cb) {
    // 提交异步操作
    // 操作完成后 somewhere: cb->complete(0)
}

// 使用
class MyCallback : public CompletionCallback {
    void complete(int result) override {
        std::cout << "Operation finished with " << result << "\n";
    }
};

async_operation(new MyCallback());
// 函数返回，但操作还在进行...
```

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

```cpp
// OSD PG状态机（简化概念）
class PG {
    enum State {
        STATE_INITIAL,
        STATE_PEERING,      // 正在建连
        STATE_ACTIVE,       // 活跃可服务
        STATE_DEGRADED,    // 降级模式
        STATE_RECOVERING,  // 正在恢复
    };
    
    State current_state;
    
    void on_event(Event* e) {
        switch (current_state) {
            case STATE_PEERING:
                if (e->type == PEERING_DONE) {
                    current_state = STATE_ACTIVE;
                    // 触发后续操作...
                }
                break;
            // ...
        }
    }
};
```

**读代码时要养成习惯：**
1. 这个请求当前在什么状态？
2. 什么事件会推进状态？
3. 状态推进后会做什么？

## 3. 线程模型

### 3.1 Ceph线程模型概览

```
┌─────────────────────────────────────────────────────┐
│                     OSD进程                          │
├─────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐ │
│  │ 网络线程    │  │ 工作线程    │  │ 磁盘IO线程   │ │
│  │ Messenger   │  │ OpWQ        │  │ FileStore   │ │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘ │
│         │                │                │        │
│         └────────────────┼────────────────┘        │
│                          ↓                         │
│                    ┌─────────┐                     │
│                    │ 消息队列│                     │
│                    │ OpQueue │                     │
│                    └─────────┘                     │
└─────────────────────────────────────────────────────┘
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
// 不是直接调用，而是发消息
class OSD {
    void handle_message(Message* m) {
        // 收到消息，放入队列
        op_queue.push(m);
    }
};

// Consumer线程从队列取消息处理
void OpWorker::run() {
    while (running) {
        Message* m = op_queue.pop();  // 可能阻塞等待
        process(m);                    // 处理消息
    }
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

// 消息传递（异步）
void handle_message(Message* m) {
    switch (m->get_type()) {
        case MSG_REQUEST:
            // 发消息给认证模块，不等待
            send_to_auth_module(new AuthMessage(m));
            break;
        case MSG_AUTH_DONE:
            // 收到认证完成消息，继续下一步
            send_to_validate_module(...);
            break;
        // ... 每个消息推进一个阶段
    }
}
```

## 5. 实践建议

### 5.1 建立"请求追踪图"

读到Ceph代码时，画图：

```
请求流程图：

客户端 --[write请求]--> OSD网络线程 --[入队]--> OpWQ工作线程
                                                       |
                                                       v
                                                   处理请求
                                                       |
                                                       v
                                                   写磁盘（异步）
                                                       |
                                                       v
                                                   回调触发
                                                       |
                                                       v
                                                   回复客户端
```

### 5.2 建立"对象关系图"

```
对象关系图：

OSDService
    ├── OSDMap        (集群状态)
    ├── Messenger     (网络通信)
    ├── ObjectStore    (存储引擎)
    └── PGReplay      (PG恢复)
        
每个PG
    ├── PGLog         (操作日志)
    ├── OSDMap        (引用，不是拥有)
    └── ObjectStore   (引用)
```

### 5.3 建立"线程分布图"

```
线程分布：

网络线程（多个）
    └── 接收消息，放入队列

工作线程（多个）
    └── 从队列取消息，处理

磁盘线程（可能有）
    └── 完成回调后，通知工作线程
```

## 6. 检查清单

完成本阶段后，你应该能：

- [ ] 理解同步与异步的区别
- [ ] 理解释为什么Ceph用异步模型
- [ ] 知道什么是回调
- [ ] 知道什么是状态机
- [ ] 能说出"请求来了 → 入队 → 异步处理 → 回调"的流程
- [ ] 读代码时会问"这个函数在哪个线程执行？"

## 下一步

这些概念理解后，再学C++语法，效率会高很多。

因为你知道：
- 为什么要学类和对象 → Ceph用对象组织代码
- 为什么要学虚函数 → Ceph用接口+实现分离
- 为什么要学回调 → Ceph到处都是异步完成
- 为什么要学线程 → Ceph多线程处理请求