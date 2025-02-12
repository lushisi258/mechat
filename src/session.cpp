#include "../include/session.hpp"
#include <iostream>

Session::Session(asio::io_context &ioc)
    : ws_(asio::make_strand(ioc)),
      heartbeat_timer_(ws_.get_executor()),
      pong_timeout_timer_(ws_.get_executor()) {}

tcp::socket &Session::Socket() {
    return ws_.next_layer();
}

void Session::Start() {
    ws_.async_accept(
        beast::bind_front_handler(&Session::OnAccept, shared_from_this()));
}

void Session::OnAccept(beast::error_code ec) {
    if (ec) {
        std::cerr << "Accept error: " << ec.message() << std::endl;
        return;
    }
    std::cout << "WebSocket connection accepted!" << std::endl;

    // 设置控制回调以处理Pong帧
    ws_.control_callback(
        [self = shared_from_this()](websocket::frame_type type, beast::string_view) {
            if (type == websocket::frame_type::pong) {
                self->OnPongReceived();
            }
        });

    StartHeartbeatTimer();
    DoRead();
}

void Session::DoRead() {
    ws_.async_read(buffer_, beast::bind_front_handler(&Session::OnRead,
                                                      shared_from_this()));
}

// 消息处理
void Session::OnRead(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec == websocket::error::closed) {
        std::cout << "WebSocket connection closed" << std::endl;
        return;
    }
    if (ec) {
        std::cerr << "Read error: " << ec.message() << std::endl;
        return;
    }

    std::cout << "Received: " << beast::make_printable(buffer_.data())
              << std::endl;
    // 清空缓冲区
    buffer_.consume(buffer_.size());

    // 收到数据后重置心跳定时器和超时定时器
    heartbeat_timer_.cancel();
    pong_timeout_timer_.cancel();
    StartHeartbeatTimer();

    DoRead();
}

void Session::StartHeartbeatTimer() {
    heartbeat_timer_.expires_after(std::chrono::seconds(30));
    heartbeat_timer_.async_wait(
        beast::bind_front_handler(&Session::OnHeartbeatTimer, shared_from_this()));
}

void Session::OnHeartbeatTimer(beast::error_code ec) {
    if (ec == asio::error::operation_aborted) {
        // 定时器被取消，忽略
        return;
    }
    if (ec) {
        std::cerr << "Heartbeat timer error: " << ec.message() << std::endl;
        return;
    }

    // 发送Ping
    ws_.async_ping(beast::websocket::ping_data{},
        beast::bind_front_handler(&Session::OnPingSent, shared_from_this()));
}

void Session::OnPingSent(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ping send error: " << ec.message() << std::endl;
        Close();
        return;
    }

    // 启动Pong超时定时器
    StartPongTimeoutTimer();
}

void Session::StartPongTimeoutTimer() {
    pong_timeout_timer_.expires_after(std::chrono::seconds(5));
    pong_timeout_timer_.async_wait(
        beast::bind_front_handler(&Session::OnPongTimeout, shared_from_this()));
}

void Session::OnPongTimeout(beast::error_code ec) {
    if (ec == asio::error::operation_aborted) {
        // 定时器被取消，忽略
        return;
    }
    if (ec) {
        std::cerr << "Pong timeout timer error: " << ec.message() << std::endl;
        return;
    }

    std::cerr << "Pong timeout. Closing connection." << std::endl;
    Close();
}

void Session::OnPongReceived() {
    // 取消Pong超时定时器
    pong_timeout_timer_.cancel();

    // 重新启动心跳定时器
    heartbeat_timer_.cancel();
    StartHeartbeatTimer();
}

void Session::Close() {
    beast::error_code ec;
    ws_.close(websocket::close_code::normal, ec);
    if (ec) {
        std::cerr << "Close error: " << ec.message() << std::endl;
    }
    heartbeat_timer_.cancel();
    pong_timeout_timer_.cancel();
}