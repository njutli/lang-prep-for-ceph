// 练习2: 智能指针使用
// 文件: exercises/smart_pointer.cpp

#include <memory>
#include <iostream>
#include <vector>
#include <string>

class Resource {
public:
    std::string name;
    Resource(const std::string& n) : name(n) {
        std::cout << "Resource " << name << " created\n";
    }
    ~Resource() {
        std::cout << "Resource " << name << " destroyed\n";
    }
    void use() { std::cout << "Using " << name << "\n"; }
};

// 自定义删除器
void custom_deleter(Resource* p) {
    std::cout << "Custom deleter for " << p->name << "\n";
    delete p;
}

int main() {
    // unique_ptr - 独占所有权
    {
        std::unique_ptr<Resource> ptr1 = std::make_unique<Resource>("res1");
        ptr1->use();
        // std::unique_ptr<Resource> ptr2 = ptr1;  // 编译错误！不能拷贝
        std::unique_ptr<Resource> ptr2 = std::move(ptr1);  // OK: 移动
        // ptr1现在是nullptr
    }  // 离开作用域自动销毁
    
    std::cout << "---\n";
    
    // shared_ptr - 共享所有权
    {
        std::shared_ptr<Resource> sp1 = std::make_shared<Resource>("res2");
        {
            std::shared_ptr<Resource> sp2 = sp1;  // 引用计数+1
            std::cout << "use_count: " << sp1.use_count() << "\n";  // 2
        }  // sp2销毁，引用计数-1
        std::cout << "use_count: " << sp1.use_count() << "\n";  // 1
    }  // 引用计数为0，资源销毁
    
    std::cout << "---\n";
    
    // weak_ptr - 观察者，不增加引用计数
    {
        std::shared_ptr<Resource> sp = std::make_shared<Resource>("res3");
        std::weak_ptr<Resource> wp = sp;
        
        std::cout << "use_count: " << sp.use_count() << "\n";  // 1
        std::cout << "weak_count: " << wp.use_count() << "\n";  // 1
        
        if (auto locked = wp.lock()) {  // lock()返回shared_ptr
            locked->use();
        }
    }  // 资源销毁
    
    std::cout << "---\n";
    
    // 自定义删除器
    {
        std::unique_ptr<Resource, void(*)(Resource*)> ptr(
            new Resource("res4"), custom_deleter);
    }
    
    // vector中存储unique_ptr
    std::vector<std::unique_ptr<Resource>> resources;
    resources.push_back(std::make_unique<Resource>("res5"));
    resources.push_back(std::make_unique<Resource>("res6"));
    
    return 0;
}