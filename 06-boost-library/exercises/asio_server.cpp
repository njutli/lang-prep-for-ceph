// server.cpp
// 编译: g++ -std=c++17 server.cpp -lboost_system -lpthread -o server
// 运行: ./server
//
// 演示: Asio Proactor 模式
//   - 监听端口，接受多个客户端连接
//   - 所有 IO 均使用 async_ 方法
//   - io_context.run() 启动事件循环后，主线程"空闲"出来等待事件

#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <array>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
const unsigned short PORT = 9876;

// ============================================================
// Session: 一个 TCP 连接 = 一个 Session 对象
// ============================================================
class Session : public std::enable_shared_from_this<Session> {
    tcp::socket socket_;
    std::array<char, 1024> read_buf_;

public:
    Session(tcp::socket s) : socket_(std::move(s)) {
        std::cout << "[Session] 新连接建立，来自 "
                  << socket_.remote_endpoint() << std::endl;
    }

    void start() {
        do_read();
    }

private:
    void do_read() {
        // 发起异步读 —— 立刻返回，主线程不被阻塞
        socket_.async_read_some(
            boost::asio::buffer(read_buf_),
            [self = shared_from_this()](
                boost::system::error_code ec, std::size_t len) {
                if (!ec) {
                    std::cout << "[Session] 读到 " << len
                              << " 字节: \""
                              << std::string(self->read_buf_.data(), len)
                              << "\"" << std::endl;
                    // 把收到的数据原样发回去（echo）
                    self->do_write(len);
                } else {
                    std::cout << "[Session] 连接关闭: " << ec.message()
                              << std::endl;
                }
            });
        // <-- async_read_some 调用完就到这里了
        //     数据还没到，但线程已经自由了
    }

    void do_write(std::size_t len) {
        // 发起异步写 —— 同样立刻返回
        boost::asio::async_write(
            socket_,
            boost::asio::buffer(read_buf_, len),
            [self = shared_from_this()](
                boost::system::error_code ec, std::size_t /*len*/) {
                if (!ec) {
                    std::cout << "[Session] 发送完成，继续等下一条..."
                              << std::endl;
                    self->do_read(); // 回到 read，形成事件链
                } else {
                    std::cout << "[Session] 发送失败: " << ec.message()
                              << std::endl;
                }
            });
    }
};

// ============================================================
// Server: 监听 + accept
// ============================================================
class Server {
    tcp::acceptor acceptor_;

public:
    Server(boost::asio::io_context& ctx, unsigned short port)
        : acceptor_(ctx, tcp::endpoint(tcp::v4(), port)) {
        std::cout << "[Server] 开始监听 0.0.0.0:" << port << std::endl;
        do_accept();
    }

private:
    void do_accept() {
        // 发起异步 accept —— 立刻返回，不阻塞
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    // 连接来了，创建 Session 并启动
                    std::make_shared<Session>(std::move(socket))->start();
                } else {
                    std::cerr << "[Server] accept 失败: " << ec.message()
                              << std::endl;
                }
                // 再次发起 accept，等待下一个客户端
                do_accept();
            });
        // <-- async_accept 调用完后就返回了
        //     此时还没有任何客户端连接，但线程已经自由了
    }
};

// ============================================================
// main: 提交 io_context.run() 后，主线程还能做什么？
// ============================================================
int main() {
    std::cout << "=== Asio Server Demo ===" << std::endl;

    boost::asio::io_context ctx;
    Server server(ctx, PORT);

    // 在后台线程跑事件循环（模拟 Ceph 的 NetworkWorker 线程）
    std::thread t([&ctx]() {
        std::cout << "[io_context] run() 启动，进入事件循环..." << std::endl;
        ctx.run(); // 阻塞在这里，但只等有 IO 事件时才工作
        std::cout << "[io_context] run() 退出（没有更多任务）" << std::endl;
    });

    // 主线程现在完全自由！可以处理其他业务逻辑
    std::cout << "[Main]   io_context.run() 已交到后台线程" << std::endl;
    std::cout << "[Main]   主线程现在是空闲的，可以..." << std::endl;
    std::cout << "[Main]     - 处理用户输入" << std::endl;
    std::cout << "[Main]     - 做计算密集型任务" << std::endl;
    std::cout << "[Main]     - 监控集群状态（类似 Ceph OSD）" << std::endl;
    std::cout << std::endl;
    std::cout << "[Main]   等待 60 秒后自动退出..." << std::endl;

    // 模拟主线程在做别的事情 —— 异步 IO 期间不阻塞
    std::this_thread::sleep_for(std::chrono::seconds(60));

    ctx.stop();   // 通知事件循环退出
    t.join();

    std::cout << "[Main]   Server 清理完成" << std::endl;
    return 0;
}
