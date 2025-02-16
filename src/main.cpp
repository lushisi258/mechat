#include "../include/server.hpp"
#include <boost/asio.hpp>
#include <iostream>

int main() {
    try {
        const unsigned short port = 2233;
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