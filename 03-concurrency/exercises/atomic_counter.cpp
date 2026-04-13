// 练习1: 线程安全计数器
// 文件: exercises/atomic_counter.cpp

#include <atomic>
#include <thread>
#include <vector>
#include <iostream>

class AtomicCounter {
private:
    std::atomic<int> counter{0};
    
public:
    void increment() {
        counter.fetch_add(1, std::memory_order_relaxed);
    }
    
    void decrement() {
        counter.fetch_sub(1, std::memory_order_relaxed);
    }
    
    int get() const {
        return counter.load(std::memory_order_relaxed);
    }
    
    // 原子递增并返回旧值
    int fetch_add(int n) {
        return counter.fetch_add(n);
    }
};

int main() {
    AtomicCounter counter;
    const int num_threads = 10;
    const int increments_per_thread = 1000;
    
    std::vector<std::thread> threads;
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&counter, increments_per_thread] {
            for (int j = 0; j < increments_per_thread; ++j) {
                counter.increment();
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "Final counter: " << counter.get() << "\n";
    std::cout << "Expected: " << num_threads * increments_per_thread << "\n";
    
    return 0;
}