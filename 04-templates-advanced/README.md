# 阶段四：模板与泛型编程

## 1. 函数模板

```cpp
// 基础函数模板
template<typename T>
T maximum(T a, T b) {
    return (a > b) ? a : b;
}

// 调用
int x = maximum(1, 2);           // 推导为int
double y = maximum(1.5, 2.5);     // 推导为double

// 模板特化
template<>
const char* maximum<const char*>(const char* a, const char* b) {
    return (strcmp(a, b) > 0) ? a : b;
}

// Ceph示例: src/common/dout.h
// 大量使用模板实现不同类型的日志输出
```

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

// Ceph示例: src/include/buffer.h 的 bufferlist
template<typename T>
class bufferlist_template { ... };
```

## 3. 模板参数

```cpp
// 类型参数
template<typename T>
class Container { T value; };

// 非类型参数（整型、指针等）
template<typename T, size_t N>
class Array { T data[N]; };

// 模板模板参数
template<template<typename> class Container, typename T>
class Wrapper { Container<T> c; };
```

## 4. 可变参数模板

```cpp
// 参数包
template<typename... Args>
void print(Args... args) {
    (std::cout << ... << args) << "\n";  // C++17折叠表达式
}

// 递归展开
template<typename T>
void print(T value) {
    std::cout << value << " ";
}

template<typename T, typename... Args>
void print(T first, Args... rest) {
    print(first);
    print(rest...);
}

// Ceph示例: src/include/encoding.h encode/decode函数
```

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

```cpp
// SFINAE：模板参数替换失败不是错误，只是排除这个重载

// 方式1: enable_if
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T>
func(T t) { return t * 2; }

// 方式2: void_t (C++17)
template<typename T, typename = std::void_t<decltype(std::declval<T>().size())>>
size_t get_size(const T& t) { return t.size(); }

// 方式3: concepts (C++20)
template<typename T>
requires std::integral<T>
T func(T t) { return t * 2; }
```

## 7. 完美转发

```cpp
template<typename T>
void wrapper(T&& arg) {
    // std::forward保持参数的值类别
    func(std::forward<T>(arg));
}

// 左值转发为左值引用
// 右值转发为右值引用

// Ceph示例: src/common/FwdAllocator.h
// 使用完美转发优化内存分配
```

## 8. 模板元编程基础

```cpp
// 编译时计算阶乘
template<int N>
struct Factorial {
    static const int value = N * Factorial<N-1>::value;
};

template<>
struct Factorial<0> {
    static const int value = 1;
};

// 使用
int x = Factorial<5>::value;  // 120

// Ceph在编解码、序列化中大量使用模板元编程
```

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