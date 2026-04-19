// 练习1: 通用swap函数
// 文件: exercises/generic_swap.cpp

#include <utility>
#include <iostream>

// 基础版本
namespace my {
    template<typename T>
    void swap(T& a, T& b) {
        T temp = std::move(a);
        a = std::move(b);
        b = std::move(temp);
    }
    
    // 数组特化
    template<typename T, size_t N>
    void swap(T (&a)[N], T (&b)[N]) {
        for (size_t i = 0; i < N; ++i) {
            my::swap(a[i], b[i]);
        }
    }
}

// 测试类
class BigObject {
public:
    std::string data;
    
    BigObject(const std::string& s) : data(s) { 
        std::cout << "Construct: " << data << "\n"; 
    }
    ~BigObject() { 
        std::cout << "Destruct: " << data << "\n"; 
    }
    
    BigObject(const BigObject& other) : data(other.data) {
        std::cout << "Copy: " << data << "\n";
    }
    
    BigObject(BigObject&& other) noexcept : data(std::move(other.data)) {
        std::cout << "Move: " << data << "\n";
        other.data = "(moved)";
    }
    
    BigObject& operator=(const BigObject& other) {
        data = other.data;
        std::cout << "Copy assign: " << data << "\n";
        return *this;
    }
    
    BigObject& operator=(BigObject&& other) noexcept {
        data = std::move(other.data);
        std::cout << "Move assign: " << data << "\n";
        other.data = "(moved)";
        return *this;
    }
};

int main() {
    {
        std::cout << "=== Swap integers ===\n";
        int a = 10, b = 20;
        std::cout << "Before: a=" << a << ", b=" << b << "\n";
        my::swap(a, b);
        std::cout << "After: a=" << a << ", b=" << b << "\n";
    }
    
    {
        std::cout << "\n=== Swap BigObject (with move) ===\n";
        BigObject a("obj_a");
        BigObject b("obj_b");
        
        std::cout << "Swapping...\n";
        my::swap(a, b);
        std::cout << "Result: a.data=" << a.data << ", b.data=" << b.data << "\n";
    }
    
    {
        std::cout << "\n=== Swap arrays ===\n";
        int arr1[] = {1, 2, 3};
        int arr2[] = {4, 5, 6};
        
        std::cout << "Before: arr1=" << arr1[0] << "," << arr1[1] << "," << arr1[2] << "\n";
        my::swap(arr1, arr2);
        std::cout << "After: arr1=" << arr1[0] << "," << arr1[1] << "," << arr1[2] << "\n";
    }
    
    return 0;
}