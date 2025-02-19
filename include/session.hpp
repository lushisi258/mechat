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
    void Start();
    void Send(const Message &msg);

  private:
    void DoRead();
    void OnAccept(beast::error_code ec);
    void OnRead(beast::error_code ec, std::size_t bytes);
    void HandleMessage(const Message &msg);

    // SSL 握手
    void DoHandshake();
    void OnHandshake(beast::error_code ec);

    // 心跳相关方法
    void StartHeartbeatTimer();
    void OnHeartbeatTimer(beast::error_code ec);
    void OnPingSent(beast::error_code ec);
    void StartPongTimeoutTimer();
    void OnPongTimeout(beast::error_code ec);
    void OnPongReceived();
    void Close();

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
