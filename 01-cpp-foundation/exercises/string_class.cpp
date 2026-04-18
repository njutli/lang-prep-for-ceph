// 练习1: 实现String类（构造、析构、拷贝、移动）
// 文件: exercises/string_class.cpp

#include <cstring>
#include <utility>
#include <iostream>

class MyString {
private:
    char* data;
    size_t length;

public:
    // 默认构造
    MyString() : data(nullptr), length(0) {}
    
    // 构造函数
    MyString(const char* str) {
        if (str) {
            length = strlen(str);
            data = new char[length + 1];
            strcpy(data, str);
        } else {
            data = nullptr;
            length = 0;
        }
    }
    
    // 析构函数
    ~MyString() {
        delete[] data;
    }
    
    // 拷贝构造
    MyString(const MyString& other) {
        length = other.length;
        if (other.data) {
            data = new char[length + 1];
            strcpy(data, other.data);
        } else {
            data = nullptr;
        }
        std::cout << "拷贝构造\n";
    }
    
    // 拷贝赋值
    MyString& operator=(const MyString& other) {
        if (this != &other) {
            delete[] data;
            length = other.length;
            if (other.data) {
                data = new char[length + 1];
                strcpy(data, other.data);
            } else {
                data = nullptr;
            }
        }
        std::cout << "拷贝赋值\n";
        return *this;
    }
    
    // 移动构造
    // noexcept: 告诉编译器此函数保证不抛异常。移动只做指针转移，不涉及内存分配，确实不会抛异常。
    // 为什么重要：std::vector 扩容时需要搬运元素，如果移动构造是 noexcept，vector 用移动；
    //   否则 vector 回退到拷贝。因为移动中途抛异常会丢数据（原对象已被修改），拷贝抛异常则原对象还完好。
    //
    //   vector 扩容过程：
    //   1. 分配更大的新内存
    //   2. 把元素从旧内存搬到新内存（移动或拷贝）
    //   3. 析构旧内存中的对象 ← 旧对象在这里被销毁
    //   4. 释放旧内存
    //
    //   移动扩容：新对象直接拿走旧对象的指针，旧对象 data 置 nullptr，
    //            然后析构旧对象（delete[] nullptr 无操作）——不存在共享资源问题
    //   拷贝扩容：每个新对象 new 一份内存拷贝内容，然后 delete[] 旧对象的内存——有额外开销
    MyString(MyString&& other) noexcept 
        : data(other.data), length(other.length) {
        other.data = nullptr;
        other.length = 0;
        std::cout << "移动构造\n";
    }
    
    // 移动赋值（同理 noexcept）
    MyString& operator=(MyString&& other) noexcept {
        if (this != &other) {
            delete[] data;
            data = other.data;
            length = other.length;
            other.data = nullptr;
            other.length = 0;
        }
        std::cout << "移动赋值\n";
        return *this;
    }
    
    const char* c_str() const { return data; }
    size_t size() const { return length; }
};

int main() {
    MyString s1("Hello");
    MyString s2 = s1;              // 拷贝构造
    MyString s3 = std::move(s1);   // 移动构造
    
    MyString s4;
    s4 = s2;                       // 拷贝赋值
    s4 = std::move(s3);            // 移动赋值
    
    return 0;
}