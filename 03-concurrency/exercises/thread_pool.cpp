// 练习3: 简单线程池
// 文件: exercises/thread_pool.cpp
//
// 核心概念解析：
// 1. Worker生命周期：在构造函数中通过 std::thread 立即启动，但因队列为空立刻进入 cv.wait() 休眠。
//    submit() 并非"启动"Worker，而是通过 cv.notify() "唤醒"已休眠的 Worker。
// 2. Future模式：submit() 立即返回 std::future（取件凭证），任务实际在后台异步执行。
//    只有调用 future.get() 时，主线程才会阻塞等待结果。

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>

class ThreadPool {
private:
    std::vector<std::thread> workers; // 工作线程池
    std::queue<std::function<void()>> tasks; // 任务队列
    std::mutex mtx;
    std::condition_variable cv;
    bool stop = false;
    
public:
    /**
     * 构造函数：启动线程
     *
     * 注意：这里并非"创建对象等待启动"，而是"创建即启动"。
     * std::thread emplace_back 的瞬间，操作系统已经创建了线程并开始运行 Lambda 代码。
     */
    ThreadPool(size_t num_threads) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                // 这是 Worker 线程的死循环生命周期
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        
                        // === Worker 的"休眠"逻辑 ===
                        // 如果队列为空 (!tasks.empty() 为假) 且 未停止，
                        // 线程会在这里"挂起"（休眠），释放锁，不占用 CPU 资源。
                        // 直到有人调用 notify_one/notify_all 唤醒它。
                        cv.wait(lock, [this] { return stop || !tasks.empty(); });
                        
                        if (stop && tasks.empty()) return; // 停止信号且无任务：下班
                        
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    // 锁已释放。Worker 在这里"干活"
                    task(); 
                }
            });
        }
    }
    
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all(); // 唤醒所有休眠的 Worker
        for (auto& worker : workers) {
            worker.join(); // 等待所有线程安全退出
        }
    }
    
    /**
     * 提交任务，返回 future
     *
     * 语法解析：
     * -> std::future<...> : 尾置返回类型。因为返回类型依赖于后面的 Args...，所以在参数列表后定义。
     * std::future : 返回值类型。代表"承诺未来会给出结果"，相当于取件小票。
     */
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> 
    {
        using return_type = typename std::invoke_result<F, Args...>::type;
        
        // === 核心打包逻辑 (俄罗斯套娃) ===
        // 1. std::bind(...): 将 "函数 f" 和 "参数 args..." 绑定在一起。
        //    把一个可能带参数的函数 (例如 int f(int, int)) 变成一个不需要参数的函数对象 (类似于 void())。
        //    std::forward 用于完美转发，保留参数的左/右值属性，避免不必要的拷贝。
        //
        // 2. std::packaged_task<return_type()>: 这是一个带特殊功能的任务包装器。
        //    当执行它时，它不仅会运行内部的函数，还会自动把**计算结果**存入内部的保险箱（Promise）。
        //
        // 3. std::make_shared<...>: 创建 shared_ptr。
        //    因为 task 稍后会被 Lambda 表达式 [task]() 捕获到队列中。
        //    使用 shared_ptr 确保任务对象在入队、执行期间不会被意外销毁，且因为 packaged_task 不可拷贝，必须用指针。
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        // 从任务包装器中取出"取件小票"（future）。
        // 这个 result 和 task 是关联的。当 task 被执行后，结果会自动出现在 result 里。
        std::future<return_type> result = task->get_future();
        
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (stop) {
                throw std::runtime_error("Cannot submit to stopped ThreadPool");
            }
            
            // 将任务放入队列。
            // 注意：这里不能直接放 task，因为 tasks 队列存的是 function<void()>。
            // 我们用一个 Lambda 捕获 shared_ptr 指针，当 Lambda 执行时，调用 (*task)() 即执行了任务。
            tasks.emplace([task]() { (*task)(); });
        }
        
        // === Worker 的"唤醒"逻辑 ===
        // 按响电铃，通知一个正在 cv.wait 中休眠的 Worker："有活干了"
        cv.notify_one();
        
        // submit 立刻返回。注意：此时任务可能还没开始执行，但调用者已经拿到了 result 小票。
        return result;
    }
};

int main() {
    // 1. 创建池子：此时 4 个 Worker 线程瞬间启动，但立刻进入 cv.wait() 休眠
    ThreadPool pool(4);
    
    std::vector<std::future<int>> results;
    
    // 2. 提交循环：
    for (int i = 0; i < 10; ++i) {
        results.push_back(pool.submit([i] {
            std::cout << "Task " << i << " running on thread " 
                      << std::this_thread::get_id() << "\n";
            return i * i; // 计算平方
        }));
    }
    
    // 3. 收集结果：
    // 这里的顺序取决于 submit 的调用顺序。
    // result.get() 是阻塞调用：如果对应的任务还没执行完，主线程会在这里卡住等待，直到拿到值。
    for (auto& result : results) {
        std::cout << "Result: " << result.get() << "\n";
    }
    
    return 0;
}