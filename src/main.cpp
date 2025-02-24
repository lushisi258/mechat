// main.cpp
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
        // json 配置
        nlohmann::json config;
        // 读取配置文件
        std::ifstream file("/home/lushisi/projects/mechat/config/config.json");
        if (!file.is_open()) {
            std::cerr << "Failed to open file" << std::endl;
        }
        // 解析配置数据
        try {
            file >> config;
        } catch (const nlohmann::json::parse_error &e) {
            std::cerr << "Parse error: " << e.what() << std::endl;
        }

        // 初始化日志系统
        IM::Logger::init(IM::Logger::WARNING, IM::Logger::DEBUG,
                         "/home/lushisi/projects/mechat/logs/network.log");

        // 启动服务器
        boost::asio::io_context ioc;
        IM::Server server(ioc, config);

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