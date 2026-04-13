// 练习4: 无锁栈实现
// 文件: exercises/lock_free_stack.cpp

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>

/**
 * 无锁栈（Lock-Free Stack）
 * 
 * 关键点：
 * 1. 使用CAS（compare_exchange）原子操作
 * 2. 理解内存序的作用
 * 3. 处理ABA问题（本例简化，实际生产需要版本号）
 */

template<typename T>
class LockFreeStack {
private:
    struct Node {
        T data;
        Node* next;
        
        explicit Node(const T& value) : data(value), next(nullptr) {}
    };
    
    std::atomic<Node*> head{nullptr};
    
public:
    LockFreeStack() = default;
    
    // 禁止拷贝
    LockFreeStack(const LockFreeStack&) = delete;
    LockFreeStack& operator=(const LockFreeStack&) = delete;
    
    // push操作：将新节点添加到栈顶
    void push(const T& value) {
        Node* new_node = new Node(value);
        
        // 步骤1：设置new_node->next指向当前head
        new_node->next = head.load(std::memory_order_relaxed);
        
        // 步骤2：CAS将head从old_head改为new_node
        // 如果失败（head被其他线程修改），重试
        while (!head.compare_exchange_weak(
            new_node->next,      // expected: 当前head的预期值
            new_node,            // desired: 新的head
            std::memory_order_release,  // 成功时的内存序
            std::memory_order_relaxed   // 失败时的内存序
        )) {
            // CAS失败，new_node->next已被更新为当前head
            // 继续重试
        }
        
        // 成功后，new_node成为新的head
    }
    
    // pop操作：从栈顶取出节点
    bool pop(T& result) {
        // 步骤1：加载当前head
        Node* old_head = head.load(std::memory_order_relaxed);
        
        // 步骤2：CAS将head从old_head改为old_head->next
        while (old_head != nullptr) {
            if (head.compare_exchange_weak(
                old_head,                          // expected
                old_head->next,                    // desired
                std::memory_order_acquire,        // 成功时的内存序
                std::memory_order_relaxed          // 失败时的内存序
            )) {
                // CAS成功
                result = std::move(old_head->data);
                
                // 注意：在实际生产环境中，这里不能立即delete
                // 因为可能有其他线程正在访问这个节点
                // 需要使用hazard pointer或其他技术
                // 这里简化处理
                delete old_head;
                return true;
            }
            // CAS失败，old_head被更新为当前head，继续重试
        }
        
        // 栈为空
        return false;
    }
    
    // 检查栈是否为空
    bool empty() const {
        return head.load(std::memory_order_relaxed) == nullptr;
    }
    
    // 析构：清理所有节点（单线程调用）
    ~LockFreeStack() {
        T dummy;
        while (pop(dummy)) {
            // 清理所有节点
        }
    }
};

// 测试函数
void test_basic_operations() {
    std::cout << "=== Test Basic Operations ===" << std::endl;
    
    LockFreeStack<int> stack;
    
    // 测试空栈
    assert(stack.empty());
    int val;
    assert(!stack.pop(val));
    
    // 测试push/pop
    stack.push(1);
    stack.push(2);
    stack.push(3);
    
    assert(!stack.empty());
    
    assert(stack.pop(val) && val == 3);
    assert(stack.pop(val) && val == 2);
    assert(stack.pop(val) && val == 1);
    assert(stack.empty());
    
    std::cout << "Basic operations: PASSED" << std::endl;
}

// 并发测试
void test_concurrent_push() {
    std::cout << "\n=== Test Concurrent Push ===" << std::endl;
    
    LockFreeStack<int> stack;
    constexpr int NUM_THREADS = 10;
    constexpr int NUM_PUSHES = 10000;
    
    std::vector<std::thread> threads;
    
    // 每个线程push NUM_PUSHES个值
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&stack, t]() {
            for (int i = 0; i < NUM_PUSHES; i++) {
                stack.push(t * NUM_PUSHES + i);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 统计pop出来的元素数量
    int count = 0;
    int val;
    while (stack.pop(val)) {
        count++;
    }
    
    std::cout << "Expected: " << NUM_THREADS * NUM_PUSHES << std::endl;
    std::cout << "Actual: " << count << std::endl;
    assert(count == NUM_THREADS * NUM_PUSHES);
    
    std::cout << "Concurrent push: PASSED" << std::endl;
}

void test_concurrent_push_pop() {
    std::cout << "\n=== Test Concurrent Push and Pop ===" << std::endl;
    
    LockFreeStack<int> stack;
    constexpr int NUM_PUSH_THREADS = 5;
    constexpr int NUM_POP_THREADS = 5;
    constexpr int NUM_ITEMS = 10000;
    
    std::atomic<int> total_pushed{0};
    std::atomic<int> total_popped{0};
    std::atomic<bool> done_pushing{false};
    
    std::vector<std::thread> push_threads;
    std::vector<std::thread> pop_threads;
    
    // Push线程
    for (int t = 0; t < NUM_PUSH_THREADS; t++) {
        push_threads.emplace_back([&]() {
            for (int i = 0; i < NUM_ITEMS; i++) {
                stack.push(1);
                total_pushed.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    // Pop线程
    for (int t = 0; t < NUM_POP_THREADS; t++) {
        pop_threads.emplace_back([&]() {
            int val;
            while (!done_pushing.load() || !stack.empty()) {
                if (stack.pop(val)) {
                    total_popped.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    // 等待push完成
    for (auto& t : push_threads) {
        t.join();
    }
    done_pushing.store(true);
    
    // 等待pop完成
    for (auto& t : pop_threads) {
        t.join();
    }
    
    // 清理剩余
    int remaining = 0;
    int val;
    while (stack.pop(val)) {
        remaining++;
    }
    
    std::cout << "Pushed: " << total_pushed.load() << std::endl;
    std::cout << "Popped: " << total_popped.load() << std::endl;
    std::cout << "Remaining: " << remaining << std::endl;
    
    assert(total_pushed.load() == NUM_PUSH_THREADS * NUM_ITEMS);
    assert(total_popped.load() + remaining == total_pushed.load());
    
    std::cout << "Concurrent push/pop: PASSED" << std::endl;
}

// CAS计数器的两种实现
void test_cas_counter() {
    std::cout << "\n=== Test CAS Counter ===" << std::endl;
    
    // 方法1：fetch_add（高效）
    std::atomic<int> counter1{0};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads1;
    for (int t = 0; t < 10; t++) {
        threads1.emplace_back([&]() {
            for (int i = 0; i < 100000; i++) {
                counter1.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& t : threads1) t.join();
    
    auto end1 = std::chrono::high_resolution_clock::now();
    
    // 方法2：CAS循环（学习用，性能较差）
    std::atomic<int> counter2{0};
    
    auto start2 = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> threads2;
    for (int t = 0; t < 10; t++) {
        threads2.emplace_back([&]() {
            for (int i = 0; i < 100000; i++) {
                int old_val = counter2.load(std::memory_order_relaxed);
                while (!counter2.compare_exchange_weak(
                    old_val, old_val + 1,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed)) {
                    // CAS失败，old_val被更新，重试
                }
            }
        });
    }
    for (auto& t : threads2) t.join();
    
    auto end2 = std::chrono::high_resolution_clock::now();
    
    std::cout << "fetch_add result: " << counter1.load() << std::endl;
    std::cout << "CAS loop result: " << counter2.load() << std::endl;
    
    auto time1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start);
    auto time2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    
    std::cout << "fetch_add time: " << time1.count() << " us" << std::endl;
    std::cout << "CAS loop time: " << time2.count() << " us" << std::endl;
    
    assert(counter1.load() == 1000000);
    assert(counter2.load() == 1000000);
    
    std::cout << "CAS counter: PASSED" << std::endl;
    std::cout << "Note: fetch_add is usually faster than CAS loop" << std::endl;
}

int main() {
    test_basic_operations();
    test_concurrent_push();
    test_concurrent_push_pop();
    test_cas_counter();
    
    std::cout << "\n========== All tests PASSED ==========" << std::endl;
    
    return 0;
}

/**
 * 编译命令：
 * g++ -std=c++17 -pthread -O2 lock_free_stack.cpp -o lock_free_stack
 * 
 * 运行：
 * ./lock_free_stack
 * 
 * 使用ThreadSanitizer检测数据竞争：
 * g++ -std=c++17 -pthread -fsanitize=thread -g lock_free_stack.cpp -o lock_free_stack_tsan
 * ./lock_free_stack_tsan
 */