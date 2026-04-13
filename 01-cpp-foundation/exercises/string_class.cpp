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
    MyString(MyString&& other) noexcept 
        : data(other.data), length(other.length) {
        other.data = nullptr;
        other.length = 0;
        std::cout << "移动构造\n";
    }
    
    // 移动赋值
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