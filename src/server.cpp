// server.cpp
#include "../include/server.hpp"

namespace IM {

Server::Server(asio::io_context &ioc, nlohmann::json config)
    : io_context_(ioc), config_(config),
      acceptor_(ioc, tcp::endpoint(tcp::v6(), config_["server"]["port"])) {
    // 载入证书
    load_server_certificate();
    // 启动服务器
    Start();
}

void Server::load_server_certificate() {
    ssl_context_.set_options(ssl::context::default_workarounds |
                             ssl::context::no_sslv2 | ssl::context::no_sslv3 |
                             ssl::context::single_dh_use);
    ssl_context_.use_certificate_chain_file(config_["ssl"]["cert"]);
    ssl_context_.use_private_key_file(config_["ssl"]["private_key"],
                                      asio::ssl::context::pem);
}

void Server::Start() {
    // 开始接受连接
    DoAccept();
}

void Server::DoAccept() {
    auto session = std::make_shared<Session>(io_context_, ssl_context_);
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