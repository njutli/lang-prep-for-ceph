# 阶段三：现代C++特性

> 学完这个阶段，你就能看懂Ceph中大部分现代C++写法

## 为什么这阶段重要？

Ceph代码大量使用现代C++特性（C++11/14/17），不理解这些会严重阻碍阅读。

## 1. auto关键字

### 1.1 自动类型推导

```cpp
// 以前
std::map<std::string, std::vector<int>>::iterator it = my_map.begin();

// 现在
auto it = my_map.begin();  // 编译器自动推导类型

// 常见用法
auto x = 42;              // int
auto s = "hello";         // const char*
auto str = std::string("hello");  // std::string
auto ptr = std::make_shared<int>(42);  // shared_ptr<int>
```

### 1.2 Ceph中的例子

```cpp
// src/osd/OSD.cc
auto osd_map = get_osdmap();   // 自动推导类型
for (auto& pg : pgs) {         // 遍历
    pg->do_something();
}
```

**核心：** `auto`让代码更简洁，但你要能看懂它推导出什么类型。

## 2. range-based for

### 2.1 遍历容器

```cpp
// C风格
for (int i = 0; i < vec.size(); i++) {
    std::cout << vec[i] << "\n";
}

// STL风格
for (std::vector<int>::iterator it = vec.begin(); it != vec.end(); ++it) {
    std::cout << *it << "\n";
}

// 现代C++（推荐）
for (const auto& x : vec) {
    std::cout << x << "\n";
}

// map遍历
std::map<std::string, int> m;
for (const auto& [key, value] : m) {  // C++17结构化绑定
    std::cout << key << ": " << value << "\n";
}
```

### 2.2 Ceph中的例子

```cpp
// 遍历所有PG
for (auto& [pgid, pg] : pg_map) {
    pg->check_state();
}
```

## 3. lambda表达式

### 3.1 基本语法

```cpp
// 最简形式
auto f = [] { return 42; };
f();  // 返回42

// 带参数
auto add = [](int a, int b) { return a + b; };
add(1, 2);  // 返回3

// 捕获外部变量
int x = 10;
auto f1 = [x](int n) { return x + n; };     // 值捕获
auto f2 = [&x](int n) { x += n; };          // 引用捕获
auto f3 = [=](int n) { return x + n; };     // 全部值捕获
auto f4 = [&](int n) { x += n; };           // 全部引用捕获
```

### 3.2 Ceph中常见的lambda用法

```cpp
// 作为回调
void process_messages(std::vector<Message*>& msgs) {
    std::for_each(msgs.begin(), msgs.end(), [](Message* m) {
        m->process();
    });
}

// 条件过滤
auto active_pgs = filter(pgs, [](const PG* pg) {
    return pg->is_active();
});

// STL算法配合
std::sort(osds.begin(), osds.end(), 
    [](const OSDInfo& a, const OSDInfo& b) {
        return a.id < b.id;
    });
```

**核心：** lambda就是匿名函数对象，常用于回调、算法参数。

## 4. nullptr

### 4.1 替代NULL

```cpp
// 旧风格
int* p = NULL;   // 不推荐
int* p = 0;      // 不推荐

// 现代
int* p = nullptr;  // 推荐，类型安全

// 函数重载场景
void f(int);
void f(int*);

f(NULL);      // 调用 f(int)，可能出错！
f(nullptr);   // 调用 f(int*)，正确
```

## 5. enum class

### 5.1 强类型枚举

```cpp
// 旧枚举（弱类型）
enum Color { Red, Green, Blue };
int c = Red;   // 可隐式转int，不安全

// 新枚举（强类型）
enum class Color { Red, Green, Blue };
Color c = Color::Red;     // 必须用作用域
int i = static_cast<int>(c);  // 必须显式转换

// Ceph中的例子
enum class OSDState {
    None,
    Initializing,
    Booting,
    Running,
    Stopping
};

OSDState state = OSDState::Running;
```

## 6. using关键字

### 6.1 类型别名

```cpp
// 旧风格
typedef std::map<std::string, std::vector<int>> StringMap;

// 新风格（更清晰）
using StringMap = std::map<std::string, std::vector<int>>;

// 模板别名（typedef做不到）
template<typename T>
using MyVector = std::vector<T>;

// 函数指针
using Callback = void(*)(int, int);
```

### 6.2 Ceph中的例子

```cpp
// src/include/buffer.h
using bufferptr = buffer::ptr;
using bufferlist = buffer::list;

// 定义类型更清晰
using OSDMapRef = ceph::ref_t<OSDMap>;
```

## 7. emplace_back

### 7.1 就地构造

```cpp
std::vector<std::pair<int, std::string>> v;

// 旧风格：先构造临时对象，再拷贝/移动
v.push_back(std::make_pair(1, "hello"));

// 新风格：直接在容器内存中构造
v.emplace_back(1, "hello");  // 更高效

// 复杂对象
class Object {
public:
    Object(int id, std::string name) : id_(id), name_(std::move(name)) {}
private:
    int id_;
    std::string name_;
};

std::vector<Object> objects;
objects.emplace_back(1, "obj1");  // 直接构造，无临时对象
```

## 8. std::function

### 8.1 函数包装器

```cpp
#include <functional>

// 可以存储任何可调用对象
std::function<int(int, int)> op;

op = [](int a, int b) { return a + b; };  // lambda
op = std::plus<int>();                      // 函数对象
op = add;                                   // 函数指针

int result = op(1, 2);  // 调用

// Ceph中用于存储回调
std::function<void(int)> completion_callback;
```

## 9. constexpr

### 9.1 编译期常量

```cpp
// 编译期计算
constexpr int factorial(int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}

constexpr int f5 = factorial(5);  // 编译期计算，结果120

// 编译期常量数组大小
constexpr int SIZE = 100;
int arr[SIZE];  // OK
```

## 10. 结构化绑定（C++17）

```cpp
// 解构pair
std::pair<int, std::string> p = {1, "hello"};
auto [id, name] = p;  // id=1, name="hello"

// 解构tuple
std::tuple<int, double, std::string> t = {1, 3.14, "pi"};
auto [a, b, c] = t;

// 解构map元素
std::map<std::string, int> m;
for (auto& [key, value] : m) {
    std::cout << key << ": " << value << "\n";
}
```

## Ceph源码阅读示例

阅读这些文件，注意现代C++特性的使用：

```
src/osd/OSD.cc              # auto, range-for, lambda
src/osd/osd_types.h         # enum class, using
src/common/dout.h            # lambda, template
src/include/buffer_fwd.h     # using, namespace
```

## 检查清单

完成本阶段后，你应该能：

- [ ] 看懂 `auto` 并知道它推导出什么类型
- [ ] 看懂 range-based for 遍历
- [ ] 看懂 lambda 捕获列表 `[=] [&] [x] [&x]`
- [ ] 理解 `nullptr` 比 `NULL` 更安全
- [ ] 理解 `enum class` 的作用域
- [ ] 看懂 `using` 类型别名
- [ ] 理解 `emplace_back` 比快在哪
- [ ] 看懂 `std::function` 的用法

## 练习

参考 `exercises/` 目录：

1. 使用auto简化复杂类型声明
2. 使用lambda实现回调
3. 使用range-for遍历容器