// server.cpp
#include "../include/server.hpp"
#include <iostream>

namespace IM {

Server::Server(asio::io_context &ioc, asio::ssl::context &ctx,
               unsigned short port)
    : io_context_(ioc), ssl_context_(ctx),
      acceptor_(ioc, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port)) {
    // 启动服务器
    Start();
}

void Server::Start() {
    // 读取JSON文件
    std::ifstream file("/home/lushisi/projects/mechat/config/config.json");
    if (!file.is_open()) {
        std::cerr << "Failed to open file" << std::endl;
    }
    // 解析JSON数据
    try {
        file >> j;
    } catch (const nlohmann::json::parse_error &e) {
        std::cerr << "Parse error: " << e.what() << std::endl;
    }

    // 初始化 SSL 上下文
    ssl_context_.set_options(
        asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 |
        asio::ssl::context::no_sslv3 | asio::ssl::context::single_dh_use);
    ssl_context_.use_certificate_chain_file(j["ssl"]["cert"]);
    ssl_context_.use_private_key_file(j["ssl"]["private_key"],
                                      asio::ssl::context::pem);
    // 开始接受连接
    DoAccept();
}

void Server::DoAccept() {
    auto session = std::make_shared<Session>(io_context_);
    acceptor_.async_accept(session->Socket(), [this, session](const auto &ec) {
        HandleAccept(session, ec);
    });
}

void Server::HandleAccept(std::shared_ptr<Session> session,
                          const boost::system::error_code &error) {
    if (!error) {
        session->Start();
    }
    DoAccept();
}

} // namespace IM