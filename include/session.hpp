// session.hpp
#pragma once
#include "message.hpp"
#include "network_logger.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace IM {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
  public:
    Session(asio::io_context &ioc);
    tcp::socket &Socket();
    void Start();
    void Send(const Message &msg);

  private:
    void DoRead();
    void OnAccept(beast::error_code ec);
    void OnRead(beast::error_code ec, std::size_t bytes);
    void HandleMessage(const Message &msg);

    // 心跳相关方法
    void StartHeartbeatTimer();
    void OnHeartbeatTimer(beast::error_code ec);
    void OnPingSent(beast::error_code ec);
    void StartPongTimeoutTimer();
    void OnPongTimeout(beast::error_code ec);
    void OnPongReceived();
    void Close();

    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    asio::steady_timer heartbeat_timer_;
    asio::steady_timer pong_timeout_timer_;
    std::string user_id_;
};

} // namespace IM