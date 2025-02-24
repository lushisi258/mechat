// server.hpp
#pragma once
#include "database_pool.hpp"
#include "session_manager.hpp"
#include "session.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <iostream>
#include <nlohmann/json.hpp>

namespace IM {

class Server {
  public:
    // 初始化服务器，传入上下文io_context_，和服务器配置config_
    Server(asio::io_context &ioc, nlohmann::json config);
    // 启动服务器
    void start();
    void stop();

  private:
    // 加载证书
    void load_server_certificate();
    // 创建数据库连接池
    void init_database_pool();
    // 接受新连接
    void do_accept();
    // 处理新连接
    void handle_accept(std::shared_ptr<Session> session,
                       const boost::system::error_code &error);

    nlohmann::json config_;
    asio::io_context &io_context_;
    ssl::context ssl_context_{asio::ssl::context::tlsv12};
    tcp::acceptor acceptor_;
    MySQLConnectionPool mysql_pool;
    RedisConnectionPool redis_pool;
    MongoDBConnectionPool mongo_pool;
};

} // namespace IM