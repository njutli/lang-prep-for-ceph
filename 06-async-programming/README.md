# 阶段六：异步编程（Seastar框架）

Ceph的新一代存储引擎Crimson使用Seastar框架实现高性能异步IO。

## 1. 同步 vs 异步

```cpp
// 同步代码：阻塞等待
std::string read_file(const std::string& path) {
    std::ifstream file(path);
    std::string content((std::istreambuf_iterator<char>(file)), 
                         std::istreambuf_iterator<char>());
    return content;  // 读取完成前线程被阻塞
}

// 异步代码：非阻塞，继续其他工作
future<std::string> read_file_async(const std::string& path);
```

## 2. Future/Promise模式

```cpp
#include <future>

// std::future (C++11)
std::future<int> compute_async() {
    return std::async([] { 
        // 耗时计算
        return 42; 
    });
}

auto f = compute_async();
// 做其他事情...
int result = f.get();  // 阻塞等待结果

// Seastar的future更强大：
// seastar::future<int> f = read_file(...).then([](std::string s) { ... });
```

## 3. 回调模式（传统异步）

```cpp
// Ceph经典版使用回调
void read_object(Completion* cb) {
    // 发起异步读取
    // ...
    // 完成后调用 cb->complete()
}

// 缺点：回调地狱
read_object(new Completion([]{
    write_object(new Completion([]{
        sync_object(new Completion([]{
            // 越来越深的嵌套...
        }));
    }));
}));
```

## 4. 链式Future（Seastar风格）

```cpp
// Seastar使用then链式调用
future<int> process() {
    return read_file("data.txt").then([](std::string content) {
        return parse_content(content);
    }).then([](ParsedData data) {
        return process_data(data);
    }).then([](Result result) {
        return result.value;
    });
}

// 每个then返回新的future，形成链
// 没有回调地狱，代码线性
```

## 5. 协程（C++20）

```cpp
#include <coroutine>
#include <cppcoro/task.hpp>

// 使用co_await的协程写法（更直观）
cppcoro::task<int> process_coro() {
    auto content = co_await read_file_async("data.txt");
    auto data = co_await parse_content_async(content);
    auto result = co_await process_data_async(data);
    co_return result.value;
}

// Seastar不使用C++20协程，有自己的实现
// 但概念相通
```

## 6. Seastar基础示例

```cpp
// Seastar示例（概念性，需要seastar库）
#include <seastar/core/app-template.hh>
#include <seastar/core/future.hh>
#include <seastar/core/sleep.hh>

using namespace seastar;

future<> process_request() {
    // 异步睡眠
    return sleep(std::chrono::seconds(1)).then([] {
        std::cout << "Processed after 1 second\n";
        return make_ready_future<>();
    });
}

int main(int argc, char** argv) {
    app_template app;
    app.run(argc, argv, [] {
        return process_request();
    });
    return 0;
}
```

## 7. 关键概念

### continuation（延续）

```cpp
// continuation是then注册的回调
// future准备好时自动执行

future<int> f = async_operation();
future<std::string> f2 = f.then([](int i) {
    return std::to_string(i);  // continuation
});
```

### 异常处理

```cpp
future<int> f = risky_operation()
    .then([](int x) { return x * 2; })
    .handle_exception([](std::exception_ptr e) {
        return -1;  // 异常时返回默认值
    });
```

### 并行执行

```cpp
// 当所有完成时返回
future<> all = when_all(
    operation1(),
    operation2(),
    operation3()
).then([](auto results) {
    // 所有结果就绪
});

// 任一完成时返回
future<> any = when_any(
    operation1(),
    operation2()
).then([](auto result) {
    // 至少一个完成
});
```

## 8. 内存管理

Seastar使用无锁数据结构和无共享架构：

```cpp
// 每个CPU核心独立处理
// 数据按核心分片，避免锁竞争

// reactor-per-core模式
// 每个核心有自己的事件循环
```

## 学习路径

1. **理解异步概念**：阻塞vs非阻塞，同步vs异步
2. **学习Future/Promise**：C++11 `std::future`
3. **协程概念**：C++20协程（可选）
4. **阅读Seastar文档**：[seastar.io](http://seastar.io)

## Ceph源码位置

```
src/crimson/                    # Crimson OSD（使用Seastar）
src/crimson/os/alienstore/      # 与经典OSD交互
src/crimson/net/                # 网络层
src/seastar/                    # Seastar副本
```

## 建议阅读顺序

1. Seastar教程(http://seastar.io/tutorial.html)
2. `src/crimson/osd/osd_operations.h` - OSD操作定义
3. `src/crimson/osd/pg.h` - Placement Group实现
4. 理解Seastar的future/continuation模型后再深入

## 注意

- Seastar是Ceph新组件，经典OSD仍用传统异步（回调+线程池）
- 学习Seastar是**可选的**，主要在阅读Crimson代码时需要
- 可以先专注于经典Ceph代码，后期再学Seastar