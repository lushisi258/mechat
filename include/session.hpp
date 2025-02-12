// session.hpp
#ifndef SESSION_HPP
#define SESSION_HPP

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <memory>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(asio::io_context &ioc);
    tcp::socket &Socket();
    void Start();

private:
    // 接收消息
    void OnAccept(beast::error_code ec);
    // 读取消息
    void DoRead();
    // 处理消息
    void OnRead(beast::error_code ec, std::size_t bytes_transferred);

    // 心跳机制相关
    // 启动心跳定时器，结束后触发回调函数 OnHeartbeatTimer()
    void StartHeartbeatTimer();
    // 心跳检测回调函数，发送Ping帧
    void OnHeartbeatTimer(beast::error_code ec);
    // Ping帧发送相关，启动Pong超时计时器
    void OnPingSent(beast::error_code ec);
    void StartPongTimeoutTimer();
    void OnPongTimeout(beast::error_code ec);
    void OnPongReceived();
    void Close();

    // 成员变量
    websocket::stream<tcp::socket> ws_;       // Websocket对象
    beast::flat_buffer buffer_;               // 缓冲区
    asio::steady_timer heartbeat_timer_;      // 心跳定时器
    asio::steady_timer pong_timeout_timer_;   // Pong响应超时定时器
};

#endif // SESSION_HPP