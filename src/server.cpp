#include "../include/server.hpp"
#include "../include/session.hpp"

// 构造函数
// 创建服务器实例，启动服务器 Start()
Server::Server(boost::asio::io_context &ioc, unsigned short port)
    : io_context_(ioc), acceptor_(ioc, {boost::asio::ip::tcp::v6(), port}) {
    Start();
}

// 服务器启动函数，开始接受用户连接
void Server::Start() { DoAccept(); }

// 接受用户连接
void Server::DoAccept() {
    // 创立一个新 session 来处理和用户的通信
    auto session = std::make_shared<Session>(io_context_);
    acceptor_.async_accept(
        session->Socket(),
        [this, session](const boost::system::error_code &error) {
            HandleAccept(session, error);
        });
}

// 处理连接
void Server::HandleAccept(std::shared_ptr<Session> session,
                          const boost::system::error_code &error) {
    if (!error) {
        session->Start();
    }
    DoAccept();
}