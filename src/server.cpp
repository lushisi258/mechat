// server.cpp
#include "../include/server.hpp"

namespace IM {

Server::Server(asio::io_context &ioc, nlohmann::json config)
    : io_context_(ioc), config_(config),
      acceptor_(ioc, tcp::endpoint(tcp::v6(), config_["server"]["port"])),
      mysql_pool(config_["database"]["mysql"]["host"].get<std::string>(),
                 config_["database"]["mysql"]["user"].get<std::string>(),
                 config_["database"]["mysql"]["password"].get<std::string>(),
                 config_["database"]["mysql"]["database"].get<std::string>(),
                 config_["database"]["mysql"]["port"].get<int>(),
                 static_cast<size_t>(
                     config_["database"]["mysql"]["max_pool_size"].get<int>())),
      redis_pool(config_["database"]["redis"]["host"].get<std::string>(),
                 config_["database"]["redis"]["port"].get<int>(),
                 static_cast<size_t>(
                     config_["database"]["redis"]["max_pool_size"].get<int>())),
      mongo_pool(config_["database"]["mongo"]["uri"].get<std::string>()) {
    // 载入证书
    load_server_certificate();
    // 初始化 session_manager 单例
    SessionManager::initialize(config_);
    // 启动服务器
    start();
}

void Server::load_server_certificate() {
    ssl_context_.set_options(ssl::context::default_workarounds |
                             ssl::context::no_sslv2 | ssl::context::no_sslv3 |
                             ssl::context::single_dh_use);
    ssl_context_.use_certificate_chain_file(config_["ssl"]["cert"]);
    ssl_context_.use_private_key_file(config_["ssl"]["private_key"],
                                      asio::ssl::context::pem);
}

void Server::start() {
    // 开始接受连接
    do_accept();
}

void Server::do_accept() {
    auto session = std::make_shared<Session>(
        io_context_, ssl_context_, mysql_pool, redis_pool, mongo_pool);
    // 异步接受新连接，并绑定到 session 的 socket
    acceptor_.async_accept(session->socket(), [this, session](const auto &ec) {
        std::cout << "Recv connection" << std::endl;
        handle_accept(session, ec);
    });
}

void Server::handle_accept(std::shared_ptr<Session> session,
                           const boost::system::error_code &error) {
    if (!error) {
        session->start();
    }
    do_accept();
}

void Server::stop() {
    acceptor_.close();
    // 关闭所有活跃 Session
    SessionManager::get_instance().shutdown();
}

} // namespace IM