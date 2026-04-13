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

// Ceph示例: src/include/buffer_fwd.h
namespace ceph {
  using bufferptr = buffer::ptr;    // 类型别名，比typedef更清晰
  using bufferlist = buffer::list;
}
```

**要点**：
- 引用必须在定义时初始化
- 引用一旦绑定不能改变
- 引用没有"空引用"概念

### 1.2 函数重载与默认参数

```cpp
// 函数重载 - 同名不同参
void log(const char* msg);
void log(const std::string& msg);
void log(int level, const char* msg);

// 默认参数
void connect(const std::string& host, int port = 8080);

// Ceph示例: src/common/dout.h
// 通过重载实现不同级别日志输出
```

### 1.3 namespace 命名空间

```cpp
// Ceph大量使用命名空间
namespace ceph {
  namespace buffer {
    class ptr;
    class list;
  }
}

// 使用
using ceph::buffer::ptr;
ceph::buffer::ptr p;
```

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

// Ceph示例: src/osd/OSD.h - OSDService继承关系
```

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
src/include/buffer_fwd.h      # namespace和using
src/common/dout.h             # 日志系统的模板实现
src/include/RefCountedObj.h   # 引用计数基类
```