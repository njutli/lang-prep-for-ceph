// 练习2: 生产者-消费者队列 (带有纳秒级时间戳，用于调试并发乱序)
// 文件: exercises/producer_consumer.cpp

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <iostream>
#include <chrono>
#include <iomanip>

// 高精度时钟用于生成唯一的时间标签
auto now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

// 带时间戳的安全打印
std::mutex print_mtx;
void safe_log(const std::string& msg) {
    std::lock_guard<std::mutex> lk(print_mtx);
    // 格式: [123456789] 消息内容
    std::cout << "[" << now_ns() << "] " << msg << "\n";
    std::cout.flush();
}

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
    BlockingQueue(size_t size = 5) : max_size(size) {}
    
    // push 返回 bool，指示是否成功放入数据
    bool push(T value) {
        std::unique_lock<std::mutex> lock(mtx);
        // 关键点：shutdown 为真时也要唤醒，否则死锁
        not_full.wait(lock, [this] { return queue.size() < max_size || shutdown; });
        
        if (shutdown) return false; // 队列已停止，不能放入
        
        queue.push(std::move(value));
        not_empty.notify_one();
        return true;
    }
    
    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        // 关键点：shutdown 为真时也要唤醒，允许消费者排空剩余数据
        not_empty.wait(lock, [this] { return !queue.empty() || shutdown; });
        
        if (queue.empty()) return false; // shutdown 且空时退出
        
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
    safe_log("[Main] Starting test...");
    BlockingQueue<int> queue(3); // 小容量
    const int num_items = 100;
    
    // 生产者
    auto producer = [&]() {
        safe_log("[Producer] Started.");
        for (int i = 0; i < num_items; ++i) {
            // 记录 push 开始
            auto t_start = now_ns();
            if (!queue.push(i)) {
                safe_log("[Producer] Stop requested.");
                return;
            }
            // 记录 push 结束后的日志
            safe_log("[Producer] Pushed: " + std::to_string(i) + " (took " + std::to_string(now_ns() - t_start) + "ns)");
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        safe_log("[Producer] Finished naturally.");
    };
    
    // 消费者
    auto consumer = [&](int id) {
        safe_log("[Consumer " + std::to_string(id) + "] Started.");
        int value;
        int count = 0;
        while (queue.pop(value)) {
            auto t_start = now_ns();
            // 处理数据（这里仅记录）
            // 模拟消费耗时
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            // 记录消费结束
            safe_log("[Consumer " + std::to_string(id) + "] Consumed: " + std::to_string(value) + " (took " + std::to_string(now_ns() - t_start) + "ns)");
            count++;
        }
        safe_log("[Consumer " + std::to_string(id) + "] Exiting.");
    };
    
    // ================= 并发原理说明 =================
    // 为什么看起来是串行调用 join，实际却是并发？
    // 1. 启动阶段（发令枪）：构造 std::thread 时，系统立即在后台创建并启动新线程。
    //    三行代码执行极快，创建完毕后三个线程已经同时在跑（并发已开始）。
    // 2. 等待阶段（join 的真正含义）：join() 不是“启动”，而是“原地阻塞等待”。
    //    当主线程执行 join() 时，主线程会卡住，但早已启动的子线程不会停，
    //    它们继续在后台并行工作，直到各自函数返回。
    // 3. 流程比喻：老板招了大厨和服务员，三人立刻开工（并发）。老板站在门口
    //    等大厨下班(join)，店里依然忙活。大厨走后老板喊停(stop)，再等服务员干完。
    // ====================================================
    std::thread t_prod(producer);
    std::thread t_c1(consumer, 1);
    std::thread t_c2(consumer, 2);
    
    // 运行 0.5s 后停止
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    safe_log("[Main] Calling stop()...");
    queue.stop();
    
    t_prod.join();
    t_c1.join();
    t_c2.join();
    
    safe_log("[Main] System halted.");
    return 0;
}