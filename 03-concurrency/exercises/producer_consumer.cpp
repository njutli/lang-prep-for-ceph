// 练习2: 生产者-消费者队列
// 文件: exercises/producer_consumer.cpp

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <iostream>
#include <vector>
#include <chrono>

template<typename T>
class BlockingQueue {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable not_empty;
    std::condition_variable not_full;
    size_t max_size;
    bool shutdown = false;
    
public:
    BlockingQueue(size_t size = 100) : max_size(size) {}
    
    void push(T value) {
        std::unique_lock<std::mutex> lock(mtx);
        not_full.wait(lock, [this] { return queue.size() < max_size || shutdown; });
        
        if (shutdown) return;
        
        queue.push(std::move(value));
        not_empty.notify_one();
    }
    
    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        not_empty.wait(lock, [this] { return !queue.empty() || shutdown; });
        
        if (queue.empty()) return false;
        
        value = std::move(queue.front());
        queue.pop();
        not_full.notify_one();
        return true;
    }
    
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            shutdown = true;
        }
        not_empty.notify_all();
        not_full.notify_all();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx));
        return queue.size();
    }
};

int main() {
    BlockingQueue<int> queue(10);
    const int num_items = 100;
    
    // 生产者
    auto producer = [&queue, num_items] {
        for (int i = 0; i < num_items; ++i) {
            queue.push(i);
            std::cout << "Produced: " << i << "\n";
        }
    };
    
    // 消费者
    auto consumer = [&queue, num_items](int id) {
        int value;
        int count = 0;
        while (queue.pop(value) && count < num_items / 2) {
            std::cout << "Consumer " << id << " consumed: " << value << "\n";
            count++;
        }
    };
    
    std::thread prod(producer);
    std::thread cons1(consumer, 1);
    std::thread cons2(consumer, 2);
    
    prod.join();
    queue.stop();
    
    cons1.join();
    cons2.join();
    
    std::cout << "Done\n";
    return 0;
}