// server.cpp
#include "../include/server.hpp"
#include <iostream>

namespace IM {

Server::Server(asio::io_context &ioc, unsigned short port)
    : io_context_(ioc), acceptor_(ioc, {asio::ip::tcp::v6(), port}) {
    Start();
}

void Server::Start() { DoAccept(); }

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