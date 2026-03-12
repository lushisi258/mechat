#include "src/WssServer.hpp"         
#include <boost/asio/io_context.hpp>

int main()
{
    boost::asio::io_context ioc;

    auto server = std::make_shared<WssServer>(ioc, 2233);
    server->run();          // 开始异步 accept

    // 阻塞直到程序退出
    ioc.run();

    return 0;
}