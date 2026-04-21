# 阶段四：模板与泛型编程

## 1. 函数模板

```cpp
// 基础函数模板：通用逻辑，覆盖 90% 的场景
template<typename T>
void process(T value) {
    std::cout << "[Generic] Processing: " << value << "\n";
}

// 模板特化：针对特定类型的“开小灶”版本
// 语法：template<> + 函数名<具体类型>(具体参数)
template<>
void process<int>(int value) {
    std::cout << "[Specialized int] Fast path for int: " << value << "\n";
}

template<>
void process<std::string>(std::string value) {
    std::cout << "[Specialized string] String len: " << value.length() << "\n";
}

// 调用
process(3.14);        // 匹配主模板
process(42);          // 匹配 int 特化
process("hello");     // 匹配 string 特化
```

**核心逻辑：**
- 没有主模板，直接写普通函数即可，不存在特化。
- 特化的目的是**统一调用接口**：无论什么类型都叫 `process(x)`，编译器自动路由到主模板或特化版。
- C++17 后，很多特化场景被 `if constexpr` + `type_traits` 取代（见第5节）。

## 2. 类模板

```cpp
template<typename T, size_t Size>
class FixedArray {
    T data[Size];
public:
    T& operator[](size_t i) { return data[i]; }
    const T& operator[](size_t i) const { return data[i]; }
    size_t size() const { return Size; }
};

// 模板参数推导（C++17）
FixedArray<int, 10> arr;

// Ceph示例: src/include/buffer.h — buffer::list 的编码系统大量使用模板
// encode<T>(const T& v, bufferlist& bl) 对不同类型进行序列化
```

## 3. 模板参数

```cpp
// 1. 类型参数：T 是具体类型（int, string 等）
template<typename T>
class SimpleContainer { T value; };

// 2. 非类型参数：N 是整型常量或指针
template<typename T, size_t N>
class FixedArray { T data[N]; };

// 3. 模板模板参数（Template Template Parameter）
// 目的：为了让容器内部自动填充 T，用户不需要重复写类型（避免 <int, vector<int>>）
// Container 不是一个类，而是一个“类模板”（模具）
template<typename T, template<typename...> class Container> 
class SmartWrapper {
    // Container<T> 自动组合
    Container<T> items; 
};

// 【场景对比】
// A. 笨办法（普通参数）：int 要写两遍，容易漏改
// Wrapper<int, std::vector<int>> w1; 

// B. 聪明办法（模板模板参数）：只要传 vector 这个模具，int 自动填入
SmartWrapper<int, std::vector> w2;       // 自动生成 std::vector<int>
SmartWrapper<string, std::list> w3;      // 自动生成 std::list<string>

// 【进阶：为什么要用 typename...？（多参数场景）】
// STL 容器通常有多个参数。比如 map 有4个参数 (Key, Value, Compare, Alloc)。
// 如果我们用死板的 template<typename> class C，匹配 map 时会报错（参数数量对不上）。
// 使用 typename... 可以“忽略”掉不关心的参数（如分配器）。

template<typename K, typename V, template<typename...> class MapType>
class MyDatabase {
    MapType<K, V> items; // 编译器自动把 K,V 填进去，剩下的用默认值
};

MyDatabase<int, std::string, std::map> db1;             // OK: 兼容 map<K,V,Comp,Alloc>
MyDatabase<int, std::string, std::unordered_map> db2;   // OK: 兼容 map<K,V,Hash,Eq,Alloc>
```

**核心逻辑：**
- 不要传入具体的车（`Car<int>`），传入**造车图纸**（`Car`）。
- `template<typename...> class Container` 中的 `...` 表示兼容有多个参数的容器（如 vector 默认有 allocator）。
- **用途**：Ceph 中封装通用数据结构（如线程安全队列）时常用此法，让调用者用起来像原生 STL 一样简洁。

## 4. 可变参数模板

### 4.1 基础用法

```cpp
// 参数包（C++17 折叠表达式）
template<typename... Args>
void print(Args... args) {
    ((std::cout << args << " "), ...) << "\n";
}

// 递归展开（C++11 经典写法）
template<typename T, typename... Args>
void print_recursive(T first, Args... rest) {
    std::cout << first << " ";
    print_recursive(rest...); // 递归调用
}

// Ceph示例: src/include/encoding.h encode/decode函数
```

#### 三个 `...` 到底在干什么？
虽然长得一样，但在模板语法中代表**三个完全不同的动作**：

| 出现位置 | 语法片段 | 作用 | 类比 |
|---|---|---|---|
| **声明期** | `template<typename... Args>` | 登记`Args`是一个**类型列表**（如 `int, double`） | 登记收件清单 |
| **定义期** | `Args... rest` | 创建`rest`变量，它是一个**参数包裹**（Pack），里面装着一串变量 | 把清单上的东西打包 |
| **调用期** | `print(rest...)` | **解包展开**：把包裹里的东西拆成逗号分隔的独立参数传给函数 | 拆开包裹，逐个发货 |

**编译器展开过程（以 `print_recursive(1, 2.5, "hi")` 为例）：**
```text
第1轮: 传入 (1, 2.5, "hi")
  T=int, first=1
  Args=(double, const char*), rest 包裹=(2.5, "hi")
  执行: 打印 1;  print_recursive(rest...) -> 编译器展开为 print_recursive(2.5, "hi");

第2轮: 传入 (2.5, "hi")
  T=double, first=2.5
  rest 包裹=("hi")
  执行: 打印 2.5; print_recursive(rest...) -> 展开为 print_recursive("hi");

第3轮: 传入 ("hi")
  T=const char*, first="hi"
  rest 包裹=() <- 空了
  执行: 打印 "hi"; print_recursive() -> 展开为 print_recursive(); (需有空参函数终止递归)
```

**总结：** 声明期登记名单 → 定义期创建包裹 → 调用期拆开包裹。三者缺一不可。

## 5. 类型特性（Type Traits）

```cpp
#include <type_traits>

// 编译时类型检查
template<typename T>
void process(T value) {
    if constexpr (std::is_integral_v<T>) {
        std::cout << "Integer: " << value << "\n";
    } else if constexpr (std::is_pointer_v<T>) {
        std::cout << "Pointer: " << *value << "\n";
    }
}

// 常用type traits
static_assert(std::is_integral_v<int>);
static_assert(std::is_pointer_v<int*>);
static_assert(std::is_same_v<int, int32_t>);

// enable_if选择性启用模板
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T>
add(T a, T b) { return a + b; }

// C++17: if constexpr
template<typename T>
auto get_value(T t) {
    if constexpr (std::is_pointer_v<T>) {
        return *t;
    } else {
        return t;
    }
}
```

## 6. SFINAE（Substitution Failure Is Not An Error）

**核心逻辑：** "如果这个模板参数对不上，**悄悄放弃**它，去试下一个，**不要报错**。"

这就像面试：如果候选人不符合要求，直接淘汰即可（SFINAE），而不是直接报警说无法处理（编译错误）。

### 为什么要用它？
编译器在决定调用哪个重载函数时，SFINAE 允许我们**过滤掉**那些不可行的函数，而不是直接罢工。

### 三种写法（从古老到现代）

#### 方式 1：`std::enable_if`（传统的“守门员”，C++11）
```cpp
// 只有当 T 是整数时，这个函数才会"出现"
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T>
func(T t) { return t * 2; }
```
**原理：** 如果 `<条件>` 为真，`enable_if` 返回有效类型；如果为假，函数签名失效。编译器看到失效，根据 SFINAE 原则会**假装没看见这个函数**，继续寻找其他匹配项。

#### 方式 2：`void_t`（能力检测，C++17）
```cpp
// 只有当 T 拥有 .size() 方法时，这个函数才生效
template<typename T, typename = std::void_t<decltype(std::declval<T>().size())>>
size_t get_size(const T& t) { return t.size(); }
```
**原理：** `std::declval<T>().size()` 是在编译期模拟创建 T 对象并调用 `.size()`。如果 T 没有这个方法，替换就会失败。编译器默默跳过此函数，而不是报错。

#### 方式 3：Concepts（概念）（现代推荐，C++20）
**SFINAE 语法太反人类，C++20 引入了 `requires` 语法直接替代。**
```cpp
// 含义极其直白：要求 T 必须是整数
template<typename T>
requires std::integral<T>
T func(T t) { return t * 2; }
```
**对比：** 
*   方式 1 和 2 写起来像“天书”，一旦报错，错误信息长得离谱。
*   方式 3 像写文章一样自然，报错信息也极具可读性（直接告诉你“T 需要满足 Integral 概念”）。

## 7. 完美转发（Perfect Forwarding）

**核心问题：** 当模板函数接收参数并想传给下一个函数时，**参数一旦有了名字（如 `arg`），右值就会退化成左值**，导致后续函数只能走“拷贝”路径，浪费性能。

### 错误示范 vs 正确示范
```cpp
// 底层函数：如果是右值，移动很快
void consume(std::string&& val) { /* 移动资源，零拷贝 */ }

// ❌ 不完美转发：参数 arg 有了名字，变成左值，强制触发拷贝
template<typename T>
void wrapper_bad(T arg) {
    consume(arg); // 编译器认为 arg 是左值，无法匹配右值引用，只能拷贝
}

// ✅ 完美转发：万能引用 + std::forward
template<typename T>
void wrapper_good(T&& arg) { // 1. T&& 同时捕获左值和右值
    consume(std::forward<T>(arg)); // 2. 恢复“出厂设置”（右值仍为右值）
}
```

### `std::forward` 会意外修改原参数吗？
**不会。** 它只是一个**“类型复原器”**，不分配内存，不篡改数据。
- **传 `const` 左值**：转发后依然是 `const&`，编译器严格禁止修改。
- **传普通左值**：本来就是允许修改的引用，完美转发只是“如实转交”，安全边界由最终接收函数的 `const` 修饰决定。
- **传右值（临时对象）**：它本就是一次性的，被 `move` 走资源是预期行为，且避免了昂贵的深拷贝。

**一句话总结：** 完美转发是透明管道。它只负责“不丢属性”，绝不越权修改。

// Ceph示例: src/common/FwdAllocator.h
// 使用完美转发优化内存分配，避免中间环节产生不必要的对象拷贝

## 8. 模板元编程基础

**核心逻辑：** 用**类型实例化**模拟循环/递归，用**静态常量**存储计算结果。所有计算在**编译期**完成，运行期零开销。

```cpp
// 1. 通用模板：定义计算规则
// 核心疑问：为什么不直接 N * Factorial<N-1>？
// 解答：Factorial<N-1> 是个"类型"（Type），不是数字。你无法让 int * 类型。
// 必须通过后缀 ::value，从该类型提取它算好的结果。
template<int N>
struct Factorial {
    // 静态常量：N * (N-1)!
    static const int value = N * Factorial<N-1>::value;
};

// 2. 模板特化（递归终止条件）
// 相当于 if (N==0) return 1;
template<>
struct Factorial<0> {
    static const int value = 1;
};

// 3. 使用
// 编译器在编译期自动推算出结果，运行期代码等效于 "int x = 120;"
int x = Factorial<5>::value; 
```

**图解（编译器推导 `Factorial<3>`）：**

| 步骤 | 编译器行为 | 结果 |
| :--- | :--- | :--- |
| 1. | 实例化 `Factorial<3>` | 需计算 `3 * Factorial<2>::value` |
| 2. | 实例化 `Factorial<2>` | 需计算 `2 * Factorial<1>::value` |
| 3. | 实例化 `Factorial<1>` | 需计算 `1 * Factorial<0>::value` |
| 4. | 匹配特化 `Factorial<0>` | 找到终止基类，`value = 1` |
| 5. | 回溯代回 | `1*1=1`, `2*1=2`, `3*2=6` |

**进阶注记（Modern C++）：**
这种 TMP 写法虽然经典，但极其晦涩。C++11 后引入了 `constexpr`，用函数替代结构体：
```cpp
constexpr int factorial(int n) { return n <= 1 ? 1 : n * factorial(n - 1); }
// int x = factorial(5); // 编译器自动在编译期算出 120
```
**注意：** 虽然语法简化了，但你在阅读 Ceph 源码（尤其是 `enable_if`、`is_integral` 等底层逻辑）时，依然会大量看到 `::value` 的影子。

## 练习

参考 `exercises/` 目录：

1. 实现通用的swap函数
2. 实现简单的optional类
3. 实现类型萃取（is_pointer, is_array等）

## Ceph源码阅读建议

```
src/include/encoding.h         # 编解码模板
src/common/dout.h               # 日志模板系统
src/include/types.h             # 类型定义
src/msg/Message.h               # 消息序列化模板
```