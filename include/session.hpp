// session.hpp
#pragma once
#include "common.hpp"
#include "logger.hpp"
#include "message.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <iostream>

namespace IM {

class Session : public std::enable_shared_from_this<Session> {
  public:
    Session(asio::io_context &ioc, asio::ssl::context &ssl_context);
    tcp::socket &Socket();
    void start();
    void send(const Message &msg);

  private:
    void do_read();
    void on_accept(beast::error_code ec);
    void on_read(beast::error_code ec, std::size_t bytes);
    void handle_message(const Message &msg);

    // SSL 握手
    void do_handshake();
    void on_handshake(beast::error_code ec);

    // 心跳相关方法
    void start_heartbeat_timer();
    void on_heartbeat_timer(beast::error_code ec);
    void on_ping_sent(beast::error_code ec);
    void start_pong_timeout_timer();
    void on_pong_timeout(beast::error_code ec);
    void on_pong_received();
    void close();

    // 使用 SSL 加密的 WebSocket 流
    boost::beast::websocket::stream<boost::asio::ssl::stream<tcp::socket>> ws_;
    beast::flat_buffer buffer_;
    asio::steady_timer heartbeat_timer_;
    asio::steady_timer pong_timeout_timer_;
    std::string user_id_;
    // 底层socket的指针
    tcp::socket &socket = Socket();
};

} // namespace IM
