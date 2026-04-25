// client.cpp
// 编译: g++ -std=c++17 client.cpp -lboost_system -o client
// 运行: ./client
//
// 演示: Asio 客户端，连接到上面的 server
//   - async_connect → async_write → async_read
//   - 展示每次 async_ 调用后"控制流"立刻返回

#include <iostream>
#include <array>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;
const unsigned short PORT = 9876;

int main() {
    std::cout << "=== Asio Client Demo ===" << std::endl;

    boost::asio::io_context ctx;
    tcp::socket socket(ctx);
    std::array<char, 1024> read_buf;

    // 目标地址
    tcp::endpoint ep(boost::asio::ip::make_address("127.0.0.1"), PORT);

    // ---- 1. 异步连接 ----
    std::cout << "[Client] 尝试连接 127.0.0.1:" << PORT << "..." << std::endl;

    socket.async_connect(ep,
        [&](boost::system::error_code ec) {
            if (ec) {
                std::cerr << "[Client] 连接失败: " << ec.message() << std::endl;
                return;
            }
            std::cout << "[Client] 连接成功!" << std::endl;

            // ---- 2. 异步写 ----
            std::string msg = "Hello from client! 你好，Ceph!";
            std::cout << "[Client] 发送: \"" << msg << "\"" << std::endl;

            boost::asio::async_write(
                socket,
                boost::asio::buffer(msg.data(), msg.size()),
                [&](boost::system::error_code ec2, std::size_t /*len*/) {
                    if (ec2) {
                        std::cerr << "[Client] 写入失败: " << ec2.message()
                                  << std::endl;
                        return;
                    }
                    std::cout << "[Client] 发送完毕，等 server 回复..."
                              << std::endl;

                    // ---- 3. 异步读 ----
                    socket.async_read_some(
                        boost::asio::buffer(read_buf),
                        [&](boost::system::error_code ec3, std::size_t len) {
                            if (ec3) {
                                std::cerr << "[Client] 读取失败: "
                                          << ec3.message() << std::endl;
                                return;
                            }
                            std::cout << "[Client] 收到回复: \""
                                      << std::string(read_buf.data(), len)
                                      << "\"" << std::endl;
                        });
                    std::cout << "[Client] async_read 已提交，线程自由了"
                              << std::endl;
                });
            std::cout << "[Client] async_write 已提交，线程自由了"
                      << std::endl;
        });

    // async_connect 已经提交，到这里时连接还没建立，
    // 但主线程已经可以往下走了
    std::cout << "[Client] async_connect 已提交，线程自由了" << std::endl;
    std::cout << "[Client] 现在调用 ctx.run() 等待所有回调触发..." << std::endl;

    ctx.run(); // 阻塞，直到所有 async 操作完成（或出错）

    std::cout << "[Client] ctx.run() 返回，程序结束" << std::endl;
    return 0;
}
