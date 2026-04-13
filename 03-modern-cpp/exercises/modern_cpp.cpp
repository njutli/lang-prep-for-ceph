// 练习：现代C++特性
// 文件: exercises/modern_cpp.cpp

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <functional>

// 练习1: auto关键字简化类型
void practice_auto() {
    std::cout << "=== Practice 1: auto ===\n";
    
    // 传统写法
    std::map<std::string, std::vector<int>> data;
    std::map<std::string, std::vector<int>>::iterator it1 = data.begin();
    
    // 使用auto
    auto it2 = data.begin();  // 更简洁
    
    // 常见auto用法
    auto x = 42;                    // int
    auto y = 3.14;                  // double
    auto s = std::string("hello");  // std::string
    auto v = std::vector<int>{1,2,3}; // std::vector<int>
    
    std::cout << "x=" << x << ", s=" << s << "\n";
}

// 练习2: range-for遍历
void practice_range_for() {
    std::cout << "\n=== Practice 2: range-for ===\n";
    
    std::vector<int> nums = {1, 2, 3, 4, 5};
    
    // 传统遍历
    for (size_t i = 0; i < nums.size(); ++i) {
        std::cout << nums[i] << " ";
    }
    std::cout << "\n";
    
    // range-for（值访问）
    for (auto n : nums) {
        std::cout << n << " ";
    }
    std::cout << "\n";
    
    // range-for（引用访问，可修改）
    for (auto& n : nums) {
        n *= 2;  // 修改原容器
    }
    
    // range-for（const引用，不可修改，高效）
    for (const auto& n : nums) {
        std::cout << n << " ";
    }
    std::cout << "\n";
    
    // map遍历（C++17结构化绑定）
    std::map<std::string, int> scores = {{"Alice", 90}, {"Bob", 85}};
    for (const auto& [name, score] : scores) {
        std::cout << name << ": " << score << "\n";
    }
}

// 练习3: lambda表达式
void practice_lambda() {
    std::cout << "\n=== Practice 3: lambda ===\n";
    
    std::vector<int> v = {3, 1, 4, 1, 5, 9, 2, 6};
    
    // 用于STL算法
    std::sort(v.begin(), v.end(), [](int a, int b) {
        return a > b;  // 降序
    });
    
    // 打印
    std::for_each(v.begin(), v.end(), [](int n) {
        std::cout << n << " ";
    });
    std::cout << "\n";
    
    // 捕获外部变量
    int threshold = 5;
    auto count_above = [threshold](const std::vector<int>& vec) {
        int count = 0;
        for (int n : vec) {
            if (n > threshold) count++;
        }
        return count;
    };
    std::cout << "Numbers above " << threshold << ": " 
              << count_above(v) << "\n";
    
    // 引用捕获（修改外部变量）
    int sum = 0;
    std::for_each(v.begin(), v.end(), [&sum](int n) {
        sum += n;
    });
    std::cout << "Sum: " << sum << "\n";
}

// 练习4: emplace_back
class Point {
public:
    int x, y;
    Point(int x, int y) : x(x), y(y) {
        std::cout << "Construct Point(" << x << "," << y << ")\n";
    }
    Point(const Point& p) : x(p.x), y(p.y) {
        std::cout << "Copy Point(" << x << "," << y << ")\n";
    }
};

void practice_emplace() {
    std::cout << "\n=== Practice 4: emplace_back ===\n";
    
    std::vector<Point> points;
    
    std::cout << "--- push_back ---\n";
    points.push_back(Point(1, 2));  // 先构造临时对象，再移动
    
    std::cout << "\n--- emplace_back ---\n";
    points.emplace_back(3, 4);  // 直接在容器中构造，无需临时对象
}

// 练习5: std::function
void practice_function() {
    std::cout << "\n=== Practice 5: std::function ===\n";
    
    // std::function可以存储任何可调用对象
    std::function<int(int, int)> op;
    
    // lambda
    op = [](int a, int b) { return a + b; };
    std::cout << "add: " << op(1, 2) << "\n";
    
    // 另一个lambda
    op = [](int a, int b) { return a * b; };
    std::cout << "mul: " << op(3, 4) << "\n";
    
    // 实际应用：存储回调
    std::function<void(const std::string&)> callback;
    callback = [](const std::string& msg) {
        std::cout << "Callback: " << msg << "\n";
    };
    callback("Hello from callback!");
}

// 练习6: enum class
enum class Color { Red, Green, Blue };
enum class Size { Small, Medium, Large };

void practice_enum_class() {
    std::cout << "\n=== Practice 6: enum class ===\n";
    
    // 强类型枚举
    Color c = Color::Red;
    
    // 不会混淆
    Size s = Size::Small;
    
    // 必须显式转换
    int color_value = static_cast<int>(c);
    std::cout << "Red = " << color_value << "\n";
    
    // 使用switch
    switch (c) {
        case Color::Red:
            std::cout << "It's red!\n";
            break;
        case Color::Green:
            std::cout << "It's green!\n";
            break;
        case Color::Blue:
            std::cout << "It's blue!\n";
            break;
    }
}

int main() {
    practice_auto();
    practice_range_for();
    practice_lambda();
    practice_emplace();
    practice_function();
    practice_enum_class();
    return 0;
}