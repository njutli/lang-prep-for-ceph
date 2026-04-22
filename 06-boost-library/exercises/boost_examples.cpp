// 练习：Boost 核心组件实战
// 文件: exercises/boost_examples.cpp

#include <iostream>
#include <string>
#include <atomic>
#include <utility>

// --- 引入 Boost 源码 (假设路径为 /home/i_ingfeng/boost_1_85_0) ---
#include <boost/container/small_vector.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/list_hook.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>

using namespace std;

// ==========================================
// 1. Boost.Container: small_vector
// ==========================================
void demo_small_vector() {
    cout << "=== 1. Boost.Container: small_vector ===" << endl;
    
    // 定义：前 3 个 int 存放在对象内部的"栈空间"中
    // 只要不超过 3 个元素，永远不会调用 malloc
    boost::container::small_vector<int, 3> sv;
    
    // 添加 3 个元素
    for (int i = 1; i <= 3; ++i) {
        sv.push_back(i * 10);
    }
    
    // 打印容量，通常初始化为 3
    cout << "[Initial] Capacity: " << sv.capacity() << " (On Stack)" << endl;

    // 添加第 4 个元素，超过容量，small_vector 内部会分配堆内存并拷贝数据
    sv.push_back(42);
    cout << "[Overflow] Capacity: " << sv.capacity() << " (Allocated on Heap)" << endl;
    
    cout << "Values: ";
    for (int val : sv) cout << val << " ";
    cout << "\n\n";
}

// ==========================================
// 2. Boost.Intrusive: intrusive list
// ==========================================

// 结构体必须继承 Hook，将链表节点指针"寄生"在数据对象内部
struct Task : public boost::intrusive::list_base_hook<> {
    int id;
    string name;
    
    Task(int id, string name) : id(id), name(std::move(name)) {
        cout << "[Task] Construct: " << this->name << endl;
    }
    
    // 即使 Task 离开容器，它也是一个独立的对象
    ~Task() {
        cout << "[Task] Destruct: " << this->name << endl;
    }
};

void demo_intrusive_list() {
    cout << "=== 2. Boost.Intrusive: intrusive list ===" << endl;
    
    boost::intrusive::list<Task> task_list;
    
    // 任务对象定义在栈上，完全掌控生命周期
    Task t1(1, "Read Data");
    Task t2(2, "Process Data");
    
    // 加入链表：直接修改对象内部的 next/prev 指针，绝无 new/delete
    task_list.push_back(t1);
    task_list.push_back(t2);
    
    cout << "Traversing: ";
    for (const auto& t : task_list) {
        cout << "[" << t.id << "] " << t.name << " ";
    }
    cout << "\n";
    
    // 关键点：在 t1 和 t2 离开作用域被销毁之前，必须先将它们移出链表！
    // 因为默认是 safe_link 模式，析构时会 Assert 检查对象是否还在链表中
    // (这也是 Ceph 中使用侵入式链表时必须严谨管理生命周期的原因)
    task_list.pop_front(); 
    task_list.pop_front();
    cout << "Tasks popped safely.\n\n";
}

// ==========================================
// 3. Boost.SmartPtr: intrusive_ptr
// ==========================================

// 自定义对象结构体
// 为了演示方便，我们让 Worker 同时支持 intrusive list (Hook)
struct Worker : public boost::intrusive::list_base_hook<> {
    std::atomic<int> ref_count{0};
    string name;
    
    Worker(string name) : name(std::move(name)) {
        // cout << "[Worker] Construct: " << this->name << endl;
    }
    
    ~Worker() {
        // intruisive_ptr 释放时，计数归零，调用 delete，进而调用此处
        cout << "[Worker] Destruct: " << this->name << " (Ref count dropped to 0)" << endl;
    }
};

// Intrusive 智能指针依赖全局函数 (ADL) 管理生命周期
void intrusive_ptr_add_ref(Worker* w) {
    ++w->ref_count;
}

void intrusive_ptr_release(Worker* w) {
    if (--w->ref_count == 0) {
        delete w;
    }
}

void demo_intrusive_ptr() {
    cout << "=== 3. Boost.SmartPtr: intrusive_ptr ===" << endl;
    
    // Boost 实现：intrusive_ptr<Worker>(ptr) 会自动调用 intrusive_ptr_add_ref(ptr)
    // 所以 Worker 初始引用计数必须为 0
    {
        auto worker = boost::intrusive_ptr<Worker>(new Worker("Worker-A"));
        cout << "[Init] Ref Count: " << worker->ref_count << endl; // 应为 1
        
        {
            // 拷贝指针
            auto worker_copy = worker;
            cout << "[Copy] Ref Count: " << worker->ref_count << endl; // 应为 2
        }
        cout << "[Copy Scope End] Ref Count: " << worker->ref_count << endl; // 回到 1
    }
    cout << "[Main Scope End] Ref Count dropped to 0, Object deleted automatically.\n" << endl;
}

int main() {
    demo_small_vector();
    demo_intrusive_list();
    demo_intrusive_ptr();
    return 0;
}
