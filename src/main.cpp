// main.cpp
#include "../include/network_logger.hpp"
#include "../include/server.hpp"
#include <atomic>
#include <boost/asio.hpp>
#include <csignal>
#include <iostream>

std::atomic<bool> stop_server(false);

void handle_signal(int) { stop_server.store(true); }

int main() {
    // 注册信号处理
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    try {
        const unsigned short port = 2233;

        // 初始化日志系统
        IM::NetworkLogger::init(IM::NetworkLogger::INFO,
                                IM::NetworkLogger::DEBUG, "logs/network.log");

        // 启动服务器
        boost::asio::io_context ioc;
        IM::Server server(ioc, port);
        std::cout << "服务器运行在端口" << port << std::endl;

        // 启动异步操作后，进入非阻塞循环
        while (!stop_server) {
            ioc.run_for(std::chrono::milliseconds(100));
        }

        // 主动停止 io_context
        ioc.stop();
        std::cout << std::endl << "服务器正常退出" << std::endl;

    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}