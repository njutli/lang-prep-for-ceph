// 练习2: 简单的Optional类
// 文件: exercises/optional.cpp

#include <iostream>
#include <type_traits>
#include <stdexcept>

template<typename T>
class Optional {
private:
    bool has_value_;
    union {
        T value_;
        char dummy_;
    };
    
public:
    // 默认构造：空值
    Optional() : has_value_(false), dummy_(0) {}
    
    // 构造：有值
    Optional(const T& value) : has_value_(true), value_(value) {}
    Optional(T&& value) : has_value_(true), value_(std::move(value)) {}
    
    // 拷贝/移动
    Optional(const Optional& other) : has_value_(other.has_value_) {
        if (has_value_) {
            new (&value_) T(other.value_);
        }
    }
    
    Optional(Optional&& other) noexcept : has_value_(other.has_value_) {
        if (has_value_) {
            new (&value_) T(std::move(other.value_));
        }
        other.has_value_ = false;
    }
    
    ~Optional() {
        if (has_value_) {
            value_.~T();
        }
    }
    
    Optional& operator=(const Optional& other) {
        if (this != &other) {
            if (has_value_) {
                if (other.has_value_) {
                    value_ = other.value_;
                } else {
                    value_.~T();
                    has_value_ = false;
                }
            } else {
                if (other.has_value_) {
                    new (&value_) T(other.value_);
                    has_value_ = true;
                }
            }
        }
        return *this;
    }
    
    Optional& operator=(Optional&& other) noexcept {
        if (this != &other) {
            if (has_value_) {
                value_.~T();
            }
            if (other.has_value_) {
                new (&value_) T(std::move(other.value_));
                has_value_ = true;
            } else {
                has_value_ = false;
            }
            other.has_value_ = false;
        }
        return *this;
    }
    
    // 查询
    bool has_value() const { return has_value_; }
    explicit operator bool() const { return has_value_; }
    
    // 访问
    T& value() & {
        if (!has_value_) throw std::runtime_error("Bad optional access");
        return value_;
    }
    
    const T& value() const& {
        if (!has_value_) throw std::runtime_error("Bad optional access");
        return value_;
    }
    
    T&& value() && {
        if (!has_value_) throw std::runtime_error("Bad optional access");
        return std::move(value_);
    }
    
    T& operator*() & { return value_; }
    const T& operator*() const& { return value_; }
    T&& operator*() && { return std::move(value_); }
    
    T* operator->() { return &value_; }
    const T* operator->() const { return &value_; }
    
    // 默认值
    T value_or(T&& default_value) const& {
        return has_value_ ? value_ : std::move(default_value);
    }
};

// 使用示例
struct User {
    int id;
    std::string name;
};

int main() {
    Optional<int> opt1;
    Optional<int> opt2 = 42;
    
    std::cout << "opt1 has_value: " << opt1.has_value() << "\n";
    std::cout << "opt2 has_value: " << opt2.has_value() << "\n";
    std::cout << "opt2 value: " << opt2.value() << "\n";
    
    if (opt2) {
        std::cout << "opt2 is truthy: " << *opt2 << "\n";
    }
    
    Optional<std::string> opt3 = std::string("hello");
    Optional<std::string> opt4;
    
    std::cout << "opt3: " << opt3.value_or("(empty)") << "\n";
    std::cout << "opt4: " << opt4.value_or("(empty)") << "\n";
    
    return 0;
}

// 函数定义需要在main外部
Optional<User> find_user(int id) {
    if (id > 0) {
        return User{id, "User" + std::to_string(id)};
    }
    return Optional<User>();
}

int main_example() {
    auto user = find_user(1);
    if (user) {
        std::cout << "User: id=" << user->id << ", name=" << user->name << "\n";
    }
    return 0;
}