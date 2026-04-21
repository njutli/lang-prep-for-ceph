// 练习：可变参数模板
// 文件: exercises/variadic_templates.cpp

#include <iostream>
#include <string>

// ==========================================
// 1. C++17 折叠表达式：最现代、最简写法
// ==========================================
// template<typename... Args> 中的 ...：声明参数包，表示 Args 可以接受任意数量的类型参数。
// void print_cpp17(Args&&... args) 中的 &&：完美转发语法，每个参数都是右值引用，允许接受左值或右值并保持原始值类别。
// ((std::cout << args << " "), ...) 中的 ...：折叠表达式，展开参数包，对每个 args 执行打印操作。
template<typename... Args>
void print_cpp17(Args&&... args) {
    ((std::cout << args << " "), ...) << "\n";
}

// ==========================================
// 2. C++11 递归展开：经典写法（帮助理解底层原理）
// ==========================================

// 递归终点：当参数包为空时，调用此函数结束递归
void print_cpp11_recursive() {
    std::cout << "\n";
}

// 递归体：每次剥开第一个参数，剩下的继续传
template<typename T, typename... Args>
void print_cpp11_recursive(T first, Args&&... rest) {
    std::cout << first << " ";
    print_cpp11_recursive(std::forward<Args>(rest)...);
}

// ==========================================
// 3. 实际业务场景：类型安全的 Debug 日志
// ==========================================
template<typename... Args>
void log_debug(const std::string& module, Args&&... args) {
    // 自动处理不同类型，无需手动转字符串或写 %d, %s
    std::cout << "[DEBUG][" << module << "] ";
    ((std::cout << args << " "), ...) << "\n";
}

// 对比：C 风格日志（容易类型不匹配崩溃）
// void log_c(const char* module, const char* fmt, ...) { ... }

int main() {
    std::cout << "=== 1. C++17 折叠表达式（一行搞定） ===\n";
    print_cpp17(1, 2.5, "hello", 'A', true);

    std::cout << "\n=== 2. C++11 递归展开（一步步剥参数） ===\n";
    print_cpp11_recursive("ID:", 10086, "Status:", "Active");

    std::cout << "\n=== 3. 实际场景：类型安全日志 ===\n";
    std::string ip = "192.168.1.100";
    int port = 8080;
    double latency = 12.5;
    
    // 自动推导 string, int, string, int, double 并打印
    log_debug("Network", "Failed to connect to", ip, ":", port, "after", latency, "ms");
    log_debug("Storage", "OSD.3", "read", 4096, "bytes successfully");

    return 0;
}