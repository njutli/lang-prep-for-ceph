// seastar_examples.cpp
// 注意: Seastar 是一个重型框架，编译需要专门的构建环境 (cmake, ninja) 和 Seastar 库。
// 这里提供标准 Seastar 风格的结构代码，用于阅读和逻辑分析。
//
// 目标: 展示 Seastar 的核心模式：Reactor, Future Chains, 和 Sharded Services。

#include <seastar/core/app-template.hh>
#include <seastar/core/future.hh>
#include <seastar/core/sleep.hh>
#include <seastar/core/sharded.hh>
#include <seastar/core/semaphore.hh>
#include <iostream>
#include <string>

using namespace seastar;

// ==========================================
// 1. 基础链式调用 (Future Chains)
// ==========================================

// 模拟一个耗时的异步读取操作
future<std::string> read_object_from_disk(const std::string& oid) {
    std::cout << "[IO] 异步读取对象: " << oid << "...\n";
    // sleep 是 Seastar 提供的异步睡眠，不会阻塞 Reactor 线程
    return sleep(std::chrono::milliseconds(100)).then([oid] {
        return oid + "_content_data";
    });
}

// 处理数据的函数
std::string transform_data(const std::string& data) {
    // 纯计算操作，直接执行
    return "Transformed: " + data;
}

void demo_chain() {
    // 核心写法：每个操作返回 future，通过 .then() 串联
    // Seastar 内部会自动调度这些 Continuation 到 Reactor 队列中
    read_object_from_disk("obj_101")
        .then([](std::string data) {
            std::cout << "[Chain] 读取完成，开始转换...\n";
            return make_ready_future<std::string>(transform_data(data));
        })
        .then([](std::string result) {
            std::cout << "[Chain] 最终结果: " << result << "\n";
        })
        .handle_exception([](std::exception_ptr e) {
            std::cerr << "[Chain] 发生错误，异步异常被捕获。\n";
        });
}

// ==========================================
// 2. Sharded 模型 (分布式/分片服务)
// ==========================================

// Ceph Crimson 中的核心服务 (如 PG, OSD) 往往继承自 peering_sharded_service
// 这里演示一个简单的跨核计数服务
struct PerfCounter {
    uint64_t count = 0;
    
    future<> start() { return make_ready_future<>(); }
    future<> stop()  { return make_ready_future<>(); }

    // 业务逻辑：增加计数
    void increment(uint64_t n) {
        count += n;
    }
};

// 演示如何使用 seastar::sharded<> 在多个 CPU 核上运行同一个服务
future<> demo_sharding(sharded<PerfCounter>& counters) {
    // counters.invoke_on_all(...) 会在每个 Reactor (CPU Core) 上执行对应的方法
    return counters.invoke_on_all([](PerfCounter& pc) {
        // 这里是在各自的 CPU 核的 Reactor 中运行的
        pc.increment(10);
        std::cout << "[Sharding] Core " << engine().cpu_id() 
                  << " 现在的计数: " << pc.count << "\n";
    });
}

// ==========================================
// 3. 信号量控制 (Semaphore) - 替代 Mutex
// ==========================================

// Seastar 不用 std::mutex (因为会阻塞 OS 线程)，而是用 semaphore
// semaphore(1) 相当于互斥锁，但它只阻塞当前协程，不阻塞 Reactor 线程
semaphore sem{1}; 

future<> demo_semaphore(int id) {
    // get_units 获取一个单位，如果获取不到则等待 (让出 CPU 给其他任务)
    return with_semaphore(sem, [&id] {
        std::cout << "[Sem] Task " << id << " 获取到了资源，开始执行...\n";
        return sleep(std::chrono::milliseconds(10)); // 模拟耗时
    }).then([id] {
        std::cout << "[Sem] Task " << id << " 释放资源。\n";
    });
}

// ==========================================
// Main: Seastar 程序入口
// ==========================================

future<> run_app() {
    std::cout << "=== Seastar Examples ===" << std::endl;
    std::cout << "当前运行在 Core: " << engine().cpu_id() << "\n\n";

    std::cout << "--- 1. Future Chain ---" << std::endl;
    demo_chain();
    
    // 注意: 在实际 Seastar 代码中，通常会将 sharded 对象作为全局或 main 类的成员
    // 这里为了演示，我们创建一个新的 sharded 实例
    
    std::cout << "\n--- 2. Sharded Service ---" << std::endl;
    sharded<PerfCounter> counters; 
    return counters.start().then([&counters] {
        return demo_sharding(counters);
    }).then([&counters] {
        return counters.stop();
    }).then([&counters] {
        return make_ready_future<>();
    }); // 为了简化，这里略过了 semaphore 的部分，专注于核心模式
}

int main(int argc, char** argv) {
    app_template app;
    
    // app.run() 会初始化 Seastar 环境，绑定 CPU 核心，并调用 run_app()
    // run_app() 返回 future<>，Seastar 会等待所有任务完成
    return app.run(argc, argv, &run_app);
}
