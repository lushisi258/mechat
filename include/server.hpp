// server.hpp
#pragma once
#include "session.hpp"
#include <boost/asio.hpp>

namespace IM {

class Server {
  public:
    // 初始化服务器
    Server(asio::io_context &ioc, unsigned short port);
    // 服务器启动
    void Start();

  private:
    // 接受用户连接
    void DoAccept();
    // 处理连接
    void HandleAccept(std::shared_ptr<Session> session,
                      const boost::system::error_code &error);

    asio::io_context &io_context_;
    asio::ip::tcp::acceptor acceptor_;
};

} // namespace IM