// 练习：函数模板与模板特化
// 文件: exercises/template_specialization.cpp

#include <iostream>
#include <string>
#include <cstring>

// 1. 主模板：通用处理逻辑
template<typename T>
void process_data(T value) {
    std::cout << "[Generic] Processing value: " << value << "\n";
}

// 2. 模板特化：专门针对 int 类型（通常用于性能优化或特殊逻辑）
// 语法：template<> + 函数名<具体类型>(具体参数列表)
template<>
void process_data<int>(int value) {
    std::cout << "[Specialized int] Processing integer: " << value << " (fast path)\n";
}

// 3. 模板特化：专门针对 std::string 类型
template<>
void process_data<std::string>(std::string value) {
    std::cout << "[Specialized string] Processing text, length: " << value.length() << "\n";
}

// 对比主模板：如果用 == 比较两个 char*，比较的是内存地址而不是内容
template<typename T>
bool is_equal(T a, T b) {
    return a == b;
}

// 4. 模板特化：针对 const char* (C字符串)
// 注意：必须在主模板声明之后定义
// 语法：template<> + 函数名<具体类型>(具体参数列表)
template<>
bool is_equal<const char*>(const char* a, const char* b) {
    return std::strcmp(a, b) == 0;
}

int main() {
    std::cout << "=== 函数模板与特化演示 ===\n\n";
    
    // 测试 process_data
    double d = 3.14;
    std::cout << "Calling process_data(d): ";
    process_data(d);      // 匹配主模板

    int i = 42;
    std::cout << "Calling process_data(i): ";
    process_data(i);      // 匹配 int 特化

    std::string s = "hello";
    std::cout << "Calling process_data(s): ";
    process_data(s);      // 匹配 string 特化

    std::cout << "\n=== 特化解决实际问题 (字符串比较) ===\n";
    const char* str1 = "test";
    const char* str2 = "test";
    const char* str3 = "other";
    
    // 主模板比较指针地址：通常返回 false（除非编译器优化了相同字面量地址）
    std::cout << "Generic template (is_equal): " << (is_equal(str1, str2) ? "true" : "false") << "\n";
    
    // 特化版本使用 strcmp：始终返回 true，因为内容相同
    std::cout << "Specialized version:       " << (is_equal<const char*>(str1, str2) ? "true" : "false") << "\n";
    std::cout << "Specialized (different):   " << (is_equal<const char*>(str1, str3) ? "true" : "false") << "\n";

    return 0;
}