// server.hpp
#pragma once
#include "session.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <nlohmann/json.hpp>

namespace IM {

class Server {
  public:
    // json 配置
    nlohmann::json j;

    // 初始化服务器
    Server(asio::io_context &ioc, asio::ssl::context &ctx, unsigned short port);
    // 启动服务器
    void Start();

  private:
    // 接受新连接
    void DoAccept();
    // 处理新连接
    void HandleAccept(std::shared_ptr<Session> session,
                      const boost::system::error_code &error);

    asio::io_context &io_context_;
    asio::ssl::context &ssl_context_;
    asio::ip::tcp::acceptor acceptor_;
};

} // namespace IM