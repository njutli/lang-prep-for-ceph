# 阶段一：C++核心语言特性

## 1. 从C到C++的过渡

### 1.1 引用 vs 指针

```cpp
// C风格指针
void swap_c(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

// C++引用 - 更安全、更直观
void swap_cpp(int &a, int &b) {
    int tmp = a;
    a = b;
    b = tmp;
}
```

**Ceph中的引用实例——类成员引用**：

C++ 引用最常见的用法是函数参数避免拷贝（`const std::string& msg`），
但在 Ceph 中还有一个重要用法——**类成员引用，作为其他成员的别名**：

```cpp
// src/osd/PG.h:1399-1405
class PG : public DoutPrefixProvider,
           public PeeringState::PeeringListener,
           public Scrub::PgScrubBeListener {
protected:
    PeeringState recovery_state;    // 实际拥有数据的成员（值语义）

    // ref to recovery_state.pool — 引用！绑定到 recovery_state 内部的 pool
    const PGPool &pool;

    // ref to recovery_state.info — 引用！绑定到 recovery_state 内部的 info
    const pg_info_t &info;
};
```

为什么这里用引用而不是指针？
- `pool` 和 `info` 一定存在（PG 构造时 recovery_state 必须先初始化）——引用不能为 null，语义更明确
- 不会改变指向——引用一旦绑定不能重新指向别的对象
- 访问时用 `pool.id` 而不是 `pool->id` ——语法更简洁

对比如果用指针：
```cpp
const PGPool *pool;       // 可能为 nullptr，每个使用者都要检查
pool->id;                  // 需要用 -> 间接访问
// 还可能被意外重新赋值指向别的对象
```

**要点**：
- 引用必须在定义时初始化——像 PG 的 `const PGPool &pool` 必须在构造函数初始化列表中绑定
- 引用一旦绑定不能改变指向——比指针更安全
- 引用没有"空引用"概念——不会出现 nullptr 的问题

### 1.2 函数重载与默认参数

```cpp
// 函数重载 - 同名不同参
void log(const char* msg);
void log(const std::string& msg);
void log(int level, const char* msg);

// 默认参数
void connect(const std::string& host, int port = 8080);
```

**Ceph中的函数重载实例——`encode()` 序列化（src/include/encoding.h）**：

Ceph 的序列化系统大量使用函数重载：同名 `encode`，根据参数类型选择不同实现：

```cpp
// 以下全是不同的重载，编译器根据参数类型自动选择
void encode(const bool &v, bufferlist& bl);              // bool
void encode(__u32 v, bufferlist& bl);                      // 整数
void encode(std::string_view s, bufferlist& bl, uint64_t features=0);  // string_view
void encode(const std::string& s, bufferlist& bl, uint64_t features=0); // string
void encode(const char *s, bufferlist& bl);               // const char*
void encode(const buffer::ptr& bp, bufferlist& bl);        // buffer::ptr
void encode(const bufferlist& s, bufferlist& bl);          // bufferlist

// 默认参数也在其中：features=0 是默认值
```

**为什么用重载而不是模板？**
- 每种类型的编码方式不同（bool 编码为 1 字节，整数编码为固定长度，字符串先编码长度再编码内容）
- 用重载让编译器自动匹配，写 `encode(x, bl)` 不用关心 `x` 是什么类型
- 对比 `dout_impl` 宏：那是**宏 + `if constexpr`**，不是函数重载

### 1.3 namespace 命名空间与 using 类型别名

```cpp
// Ceph的 namespace 结构（src/include/buffer.h, buffer_fwd.h）
namespace ceph {                          // 顶层命名空间
  namespace buffer {                      // buffer 子命名空间
    inline namespace v15_2_0 {            // ABI 版本内联命名空间
      class raw { ... };                  // 底层数据缓冲区
      class ptr { raw *_raw; unsigned _off, _len; };
      class list { ... };                 // ptr 的链表
    }
  }

  // using 类型别名（src/include/buffer_fwd.h）
  using bufferptr  = buffer::ptr;         // C++11: 等价于 typedef buffer::ptr bufferptr
  using bufferlist  = buffer::list;
  using bufferhash  = buffer::hash;
}

// 使用方式——两种写法等价：
ceph::buffer::ptr p1;       // 完整路径
ceph::bufferptr p2;         // 用类型别名，更简洁

// 其他代码中常见做法（src/include/rados/librados.hpp）
namespace librados {
  using ceph::bufferlist;   // 把 bufferlist 引入 librados 命名空间
}

// 效果：在 librados 内部，bufferlist 就是 ceph::bufferlist
// 所以 librados 的 API 可以直接写 bufferlist，不用写 ceph::buffer::list
namespace librados {
  class IoCtx {
  public:
      int write(const std::string& oid, bufferlist& bl, size_t len, uint64_t off);
      // ↑ 等价于 int write(..., ceph::buffer::list& bl, ...);
      //   用户调用时也只需 librados::bufferlist
  };
}

// 外部用户代码
#include <rados/librados.hpp>
int main() {
    librados::bufferlist bl;    // 直接用，不需要知道 ceph::buffer::list
    bl.append("hello");
}
```

**关键概念**：
- `namespace ceph { namespace buffer { ... } }` — 嵌套命名空间，防止名字冲突
- `inline namespace v15_2_0` — 内联命名空间，用于 ABI 版本控制。版本切换时改一处即可
- `using bufferptr = buffer::ptr` — **类型别名**，不是引用！它只是给类型起个短名字
- `using ceph::bufferlist` — **using 声明**，把已有名字引入当前作用域（和上面的 `using =` 不同）

### 1.4 bool类型与nullptr

```cpp
// C: 用int表示真假
int is_valid = 1;

// C++: bool类型
bool is_valid = true;

// C: NULL是宏定义 (void*)0
int *p = NULL;

// C++: nullptr是关键字，类型安全
int *p = nullptr;
```

## 2. 面向对象编程

### 2.1 类与结构体

```cpp
// C++中struct和class区别仅在于默认访问权限
struct Point {      // 默认public
    int x, y;
};

class Point {       // 默认private
    int x, y;
public:
    Point(int x, int y) : x(x), y(y) {}  // 构造函数初始化列表
    int getX() const { return x; }       // const成员函数
};
```

### 2.2 构造函数与析构函数

```cpp
class Resource {
    char *data;
public:
    Resource(size_t size) : data(new char[size]) {  // 构造
        std::cout << "Resource allocated\n";
    }
    
    ~Resource() {                 // 析构
        delete[] data;
        std::cout << "Resource freed\n";
    }
    
    // C++11: 禁止拷贝
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
};
```

### 2.3 继承与多态

```cpp
class OSDOp {
public:
    virtual void execute() = 0;    // 纯虚函数
    virtual ~OSDOp() {}            // 虚析构函数（重要！）
};

class ReadOp : public OSDOp {
public:
    void execute() override {       // override关键字(C++11)
        // 具体实现
    }
};
```

**Ceph中的继承与多态实例——PGBackend 和 Listener 接口**

Ceph 大量使用"接口+实现"模式。以下来自真实源码：

```cpp
// src/osd/PGBackend.h — 接口类（纯虚函数）
class PGBackend {
public:
    virtual bool can_handle_while_inactive(OpRequestRef op) = 0;
    virtual bool _handle_message(OpRequestRef op) = 0;
    virtual void on_change() = 0;
    virtual void clear_recovery_state() = 0;
    // ... 更多纯虚函数

    // 内嵌 Listener 接口
    class Listener {
    public:
        virtual void on_local_recover(
            const hobject_t &oid,
            const ObjectRecoveryInfo &recovery_info,
            ObjectContextRef obc,
            bool is_delete,
            ObjectStore::Transaction *t) = 0;
        virtual void send_message(int to_osd, Message *m) = 0;
        // ...
        virtual ~Listener() {}
    };

    Listener *parent;   // ← 指向 Listener 实现（即 PrimaryLogPG）
    Listener *get_parent() const { return parent; }

    PGBackend(CephContext* cct, Listener *l, ...)
        : parent(l) {}   // ← 构造时传入 Listener
};

// src/osd/ReplicatedBackend.h — 副本模式实现
class ReplicatedBackend : public PGBackend {
    bool can_handle_while_inactive(OpRequestRef op) override;
    bool _handle_message(OpRequestRef op) override;
    // ...
};

// src/osd/PG.h — PG 继承多个接口
class PG : public DoutPrefixProvider,
           public PeeringState::PeeringListener,
           public Scrub::PgScrubBeListener { ... };

// src/osd/PrimaryLogPG.h — 同时实现 Listener 接口
class PrimaryLogPG : public PG,
                     public PGBackend::Listener,
                     public ECListener {
    boost::scoped_ptr<PGBackend> pgbackend;
    // ...
};
```

**"反向接口"如何工作——完整的类关系与调用链**

**构造过程**是最初的动作，由此建立了两个对象之间的双向指针：

```
构造顺序：

1. 创建 PrimaryLogPG 对象
   │
2. PrimaryLogPG 构造函数调用 PGBackend::build_pg_backend()
   │  传入 this（即 PrimaryLogPG*），作为 Listener* 参数
   │
3. build_pg_backend() 根据 pool.type 创建 ReplicatedBackend
   │  把 Listener* (PrimaryLogPG 的 this) 传给 ReplicatedBackend 构造函数
   │
4. ReplicatedBackend 构造函数委托给基类 PGBackend
   │  PGBackend 构造函数把 Listener* 保存为 parent 成员
   │
5. build_pg_backend() 返回 ReplicatedBackend*，赋给 PrimaryLogPG::pgbackend
   │
   ▼
   最终状态：
   PrimaryLogPG.pgbackend ──→ ReplicatedBackend 对象
   ReplicatedBackend.parent ──→ PrimaryLogPG 对象（通过 Listener*）
```

对应源码：

```cpp
// 步骤 1-2: src/osd/PrimaryLogPG.cc:1784
PrimaryLogPG::PrimaryLogPG(...)
    : PG(o, curmap, _pool, p),
      pgbackend(
        PGBackend::build_pg_backend(
          _pool.info, ec_profile,
          this,          // ← 传入 PrimaryLogPG 的 this 作为 Listener*
          coll_t(p), ch, o->store, cct, ec_extent_cache_lru))

// 步骤 3: src/osd/PGBackend.cc:783
PGBackend* PGBackend::build_pg_backend(..., Listener *l, ...) {
    return new ReplicatedBackend(l, ...);  // l = PrimaryLogPG 的 this
}

// 步骤 4: src/osd/PGBackend.h:324
PGBackend(CephContext* cct, Listener *l, ...)
    : parent(l) {}   // parent 保存了 PrimaryLogPG 的指针

// 步骤 5: pgbackend 成员保存 ReplicatedBackend 的地址
```

三个类的继承与组合关系：

```
┌─────────────────────────────────────────────────────────────────┐
│                         类继承关系                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  PGBackend (接口基类)                                            │
│    ├── ReplicatedBackend (副本模式实现)                          │
│    └── ECSwitch / ECBackend (纠删码实现)                        │
│                                                                 │
│  PGBackend::Listener (回调接口，定义在 PGBackend 内部)           │
│    └── PrimaryLogPG 实现此接口                                  │
│                                                                 │
│  PG ← PrimaryLogPG (继承 PG + Listener + ...)                  │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

运行时对象关系——构造完成后两个对象互相持有对方指针：

```
PrimaryLogPG 对象                          ReplicatedBackend 对象
┌─────────────────────────────┐            ┌─────────────────────────────┐
│                             │            │                             │
│  pgbackend ─────────────────┼──────────→│ (PGBackend 基类部分)        │
│  (类型: PGBackend*)         │            │                             │
│                             │            │  parent ────────────────────┼──┐
│                             │            │  (类型: Listener*)          │  │
│                             │            │                             │  │
│  实现 Listener 接口:         │            │                             │  │
│    on_local_recover()       │◄───────────┤                             │  │
│    send_message_osd_cluster │←───────────┤                             │  │
│    pgb_is_primary()         │←───────────┤                             │  │
│                             │            │                             │  │
└─────────────────────────────┘            └─────────────────────────────┘  │
       ▲                                                                   │
       │  parent 指向 PrimaryLogPG 对象                                    │
       └───────────────────────────────────────────────────────────────────┘
```

**正向调用：PrimaryLogPG → ReplicatedBackend**（通过 pgbackend 指针）

```cpp
// src/osd/PrimaryLogPG.cc:1927
bool handled = pgbackend->handle_message(op);
// pgbackend 是 PGBackend*，实际指向 ReplicatedBackend
// 虚函数调用 → ReplicatedBackend::handle_message()

// src/osd/PrimaryLogPG.cc:634
PGBackend::RecoveryHandle *h = pgbackend->open_recovery_op();
// → ReplicatedBackend::open_recovery_op()

// src/osd/PrimaryLogPG.cc:11560
pgbackend->submit_transaction(...);
// → ReplicatedBackend::submit_transaction()
```

**反向回调：ReplicatedBackend → PrimaryLogPG**（通过 parent 指针）

```cpp
// src/osd/ReplicatedBackend.cc:2100
// 本地数据恢复完成后，通知 PG
get_parent()->on_local_recover(
    hoid, pull_info.recovery_info, pull_info.obc, false, t);
// get_parent() 返回 Listener*，实际指向 PrimaryLogPG 对象
// 虚函数调用 → PrimaryLogPG::on_local_recover()（src/osd/PrimaryLogPG.cc:403）

// src/osd/ReplicatedBackend.cc:531
// 需要发消息给其他 OSD
parent->send_message_osd_cluster(i.osd, pct_update, get_osdmap_epoch());
// → PrimaryLogPG::send_message_osd_cluster()

// src/osd/PGBackend.h:331
// PGBackend 基类本身也会通过 parent 回调
bool is_primary() const { return get_parent()->pgb_is_primary(); }
// → PrimaryLogPG::pgb_is_primary()
```

**为什么这样设计？**
- PrimaryLogPG 通过 `pgbackend`（PGBackend*）调用后端，不知道是 ReplicatedBackend 还是 ECBackend
- ReplicatedBackend 通过 `parent`（Listener*）回调 PG，不知道是 PrimaryLogPG 还是别的类
- 两个类互相不知道对方的具体类型，只通过虚函数接口通信，完全解耦

## 3. RAII资源管理（核心概念）

### 3.1 为什么需要RAII

```cpp
// C风格 - 容易资源泄漏
void process() {
    FILE* f = fopen("data.txt", "r");
    if (!f) return;        // 忘记关闭！
    // ... 可能抛异常 ...
    fclose(f);
}

// C++ RAII - 自动管理
void process() {
    std::ifstream f("data.txt");
    if (!f) return;
    // ... 可能抛异常 ...
    // 离开作用域自动关闭
}

// Ceph: bufferlist, RefCountedObject等都采用RAII
```

### 3.2 智能指针

```cpp
// unique_ptr - 独占所有权
std::unique_ptr<File> file(new File());
std::unique_ptr<File> file2 = std::move(file);  // 转移所有权

// shared_ptr - 共享所有权（引用计数）
std::shared_ptr<Connection> conn = std::make_shared<Connection>();

// weak_ptr - 不增加引用计数，解决循环引用
std::weak_ptr<Connection> weak_conn = conn;
if (auto locked = weak_conn.lock()) {  // lock()返回shared_ptr
    // 使用locked
}

// Ceph示例: src/include/RefCountedObj.h
// 很多Ceph对象继承RefCountedObject，使用intrusive_ptr
```

## 4. 移动语义

```cpp
// 左值 vs 右值
std::string s1 = "hello";      // s1是左值
std::string s2 = std::move(s1); // 移动构造，s1变成空

// 避免不必要的拷贝
std::vector<std::string> vec;
std::string s = "data";
vec.push_back(s);              // 拷贝
vec.push_back(std::move(s));   // 移动（更高效）

// Ceph大量使用移动语义优化性能
```

## 练习

参考 `exercises/` 目录完成以下练习：

1. 实现 `String` 类（构造、析构、拷贝、移动）
2. 智能指针使用：实现一个简单的资源管理器
3. 使用RAII封装文件操作

## Ceph源码阅读建议

阅读以下文件理解C++基础特性：
```
src/include/buffer_fwd.h      # namespace、using类型别名、forward declaration
src/include/buffer.h          # namespace嵌套、inline namespace、class定义
src/common/buffer.cc          # buffer::ptr的拷贝构造/析构/引用计数实现
src/include/encoding.h       # 函数重载：encode() 针对不同类型的重载
src/common/dout.h              # 宏 + 模板 + if constexpr（不是函数重载！）
src/include/RefCountedObj.h    # 引用计数基类
```