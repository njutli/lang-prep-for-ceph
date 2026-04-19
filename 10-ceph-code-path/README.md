# 阶段八：Ceph源码实战

> 带着语言能力进入Ceph源码，边看边补语言细节

## 学习方法

ChatGPT建议的方法非常好：

> 每学完一块，就回到 Ceph 源码里找对应例子

## 1. 入门代码路径

按这个顺序读，难度递增：

```
第一阶段：基础数据结构
src/include/buffer.h           # bufferlist核心数据结构
src/include/buffer_fwd.h       # 类型定义
src/include/types.h            # 基础类型
src/common/encoding.h          # 序列化

第二阶段：通用工具
src/common/dout.h              # 日志系统
src/common/config.h            # 配置系统
src/include/RefCountedObj.h   # 引用计数基类

第三阶段：核心组件
src/osd/osd_types.h            # OSD类型定义（重要！）
src/osd/OSD.h                  # OSD主类
src/osd/PG.h                   # Placement Group类
src/msg/Message.h              # 消息基类

第四阶段：数据路径（按请求流）
src/osd/OSD::handle_message()  # 消息入口
src/osd/PG::do_op()            # 操作执行
src/osd/PrimaryLogPG::do_op()   # 具体处理
```

## 2. 阅读技巧：建立三张图

### 2.1 对象关系图

画出谁创建谁、谁引用谁：

```
OSD
  ├── OSDMap        (集群状态，shared_ptr引用)
  ├── Messenger     (网络通信)
  └── ObjectStore   (存储引擎)
      └── BlueStore (具体实现)

PG
  ├── OSDMapRef     (引用OSDMap)
  ├── PGLog         (操作日志)
  └── ObjectStore   (引用)
```

### 2.2 线程分布图

标记每个组件运行的线程：

```
┌──────────────────────────────────────────────┐
│                   OSD进程                      │
├──────────────────────────────────────────────┤
│ 网络线程(Messenger)                           │
│   └── 接收消息，放入OpQueue                    │
│                                              │
│ 工作线程(OpWQ)                                │
│   └── 从OpQueue取请求，调用PG::do_op          │
│                                              │
│ 回调线程(Finisher)                            │
│   └── 执行异步完成回调                         │
│                                              │
│ 磁盘线程                                      │
│   └── 处理磁盘IO完成                           │
└──────────────────────────────────────────────┘
```

### 2.3 请求流程图

追踪一个写请求的完整路径：

```
客户端.write(数据)
    │
    v
Messenger.handle_message()
    │
    v
入队 → OpWQ
    │
    v
工作线程取出
    │
    v
OSD::handle_message()
    │
    v
找到目标PG
    │
    v
PG::do_op()
    │
    v
PrimaryLogPG::do_op()
    │
    v
生成事务
    │
    v
ObjectStore->queue_transaction()
    │
    v
写入完成回调
    │
    v
回复客户端
```

## 3. 语言特性映射到Ceph

| 语言特性 | Ceph中的例子 |
|---------|-------------|
| 类/继承 | `Message`是基类，`MOSDOp`等是子类 |
| 虚函数 | `Message::decode_payload()` |
| 智能指针 | `OSDMapRef = ceph::ref_t<OSDMap>` |
| STL容器 | `std::map<pg_t, PGRef> pg_map` |
| lambda | 日志过滤、回调注册 |
| 模板 | `encode/decode`系列函数 |
| 线程/锁 | `ceph::mutex`, `ceph::condition_variable` |
| 回调 | `Context`, `Finisher` |

## 4. 实践任务清单

### 任务1：找到消息入口
```
位置: src/osd/OSD.cc
目标: 找到 OSD::handle_message() 函数
问题: 这个函数做什么？消息从哪来？
```

### 任务2：追踪OSDMap的使用
```
位置: src/osd/OSD.h, src/osd/OSDMap.h
目标: 理解OSDMap的生命周期
问题: 
  - OSDMap何时创建？
  - 谁持有引用？
  - 何时更新？
```

### 任务3：理解bufferlist
```
位置: src/include/buffer.h
目标: 理解Ceph核心数据结构
问题:
  - bufferptr和bufferlist是什么关系？
  - 数据如何组织？
  - 为什么要这样设计？（零拷贝）
```

### 任务4：追踪写请求
```
位置: src/osd/PrimaryLogPG.cc
目标: 找到写请求处理路径
函数: PrimaryLogPG::do_op()
问题:
  - 写请求如何变成ObjectStore事务？
  - 完成后如何回调？
```

### 任务5：找到日志输出
```
位置: src/common/dout.h
目标: 理解Ceph日志宏
命令: grep -r "dout" src/osd/ | head -20
问题:
  - dout() 和 derr() 有什么区别？
  - 日志级别怎么控制？
```

## 5. 调试技巧

### 5.1 使用日志

```bash
# 调整日志级别
ceph config set osd debug_osd 20

# 查看日志
journalctl -u ceph-osd@0 -f

# 过滤日志
grep "do_op" /var/log/ceph/ceph-osd.0.log | tail -100
```

### 5.2 使用GDB

```bash
# 启动调试
gdb --args /usr/bin/ceph-osd -i 0 --conf /etc/ceph/ceph.conf

# 常用命令 break OSD::handle_message
(gdb) continue
(gdb) backtrace
(gdb) print *osdmap
(gdb) info threads
```

### 5.3 加打印

```cpp
// 在关键位置加日志
void OSD::handle_message(Message *m) {
    dout(10) << "handle_message: " << m->get_type() << dendl;
    // ...
}

// 重新编译
cd build && ninja
```

## 6. 常见问题

**Q: 代码太多，从哪开始？**
A: 从 OSD::handle_message() 入口开始，追踪一个简单读请求。

**Q: 看到不懂的语法怎么办？**
A: 先记下来，继续看逻辑。回头查语法手册补。

**Q: 很多类互相调用，看不明白？**
A: 画对象关系图，who owns who，who references who。

**Q: 回调机制太复杂？**
A: 找Finisher类，理解"提交任务→队列→回调执行"流程。

## 7. 进阶路径

完成基础后，可以深入：

```
存储引擎
├── src/os/bluestore/        # BlueStore实现
├── src/os/bluestore/BlueFS/ # 文件系统层
└── src/os/bluestore/Allocator.cc

网络层
├── src/msg/                 # 消息框架
├── src/msg/async/           # 异步网络实现
└── src/crimson/net/         # 新版网络（Seastar）

分布式协议
├── src/mon/                  # Monitor
├── src/osd/PG.cc            # PG状态机
└── src/osd/PeeringState.cc  # Peering协议
```

## 8. 检查清单

完成本阶段后，你应该能：

- [ ] 画出OSD的对象关系图
- [ ] 画出OSD的线程分布图
- [ ] 追踪一个读请求从入口到返回
- [ ] 理解bufferlist的设计目的
- [ ] 知道在哪加日志调试
- [ ] 找到OSDMap的定义和使用位置