// session.hpp
#pragma once
#include "common.hpp"
#include "database_pool.hpp"
#include "logger.hpp"
#include "message.hpp"
#include "session_manager.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <iostream>

namespace IM {

class Session : public std::enable_shared_from_this<Session> {
  public:
    // 使用 SSL 加密的 WebSocket 流
    boost::beast::websocket::stream<boost::asio::ssl::stream<tcp::socket>> ws_;
    int user_id_;
    beast::flat_buffer buffer_;
    asio::steady_timer heartbeat_timer_;
    asio::steady_timer pong_timeout_timer_;
    // 数据库连接实例
    MySQLConnectionPool &mysql_pool_;
    RedisConnectionPool &redis_pool_;
    MongoDBConnectionPool &mongo_pool_;

    Session(asio::io_context &ioc, asio::ssl::context &ssl_context,
            MySQLConnectionPool &mysql_pool, RedisConnectionPool &redis_pool,
            MongoDBConnectionPool &mongo_pool);
    void start();
    void send(const Message &msg);
    tcp::socket &socket();

  private:
    void do_read();
    void on_accept(beast::error_code ec);
    void on_read(beast::error_code ec, std::size_t bytes);
    void handle_message(const Message &msg);
    void close();
    // 消息处理函数
    // int recv_text_msg(const Message &msg);
    int recv_register_msg(const Message &msg);
    int recv_login_msg(const Message &msg);
    // int recv_logout_msg(const Message &msg);
    // SSL 握手
    void do_handshake();
    void on_handshake(beast::error_code ec);
    // 心跳相关函数
    void start_heartbeat_timer();
    void on_heartbeat_timer(beast::error_code ec);
    void on_ping_sent(beast::error_code ec);
    void start_pong_timeout_timer();
    void on_pong_timeout(beast::error_code ec);
    void on_pong_received();
};

} // namespace IM
