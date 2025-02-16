#include "../include/network_logger.hpp"
#include "../include/server.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        const unsigned short port = 2233;

        // 初始化日志系统
        IM::NetworkLogger::init(IM::NetworkLogger::INFO,  // 控制台显示INFO级别
                                IM::NetworkLogger::DEBUG, // 文件记录DEBUG级别
                                "logs/network.log");

        boost::asio::io_context ioc;
        IM::Server server(ioc, port);
        std::cout << "Server listening on port " << port << std::endl;
        ioc.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}