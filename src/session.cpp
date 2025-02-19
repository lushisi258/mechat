// session.cpp
#include "../include/session.hpp"
#include "../include/session_manager.hpp"

namespace IM {

Session::Session(asio::io_context &ioc, asio::ssl::context &ssl_context)
    : ws_(asio::make_strand(ioc), ssl_context),
      heartbeat_timer_(ws_.get_executor()),
      pong_timeout_timer_(ws_.get_executor()) {}

tcp::socket &Session::Socket() {
    return ws_.next_layer().next_layer();
} // 返回底层 socket

void Session::Start() {
    // SSL 握手
    DoHandshake();
}

void Session::DoHandshake() {
    // 启动 SSL 握手
    ws_.next_layer().async_handshake(
        boost::asio::ssl::stream_base::server,
        beast::bind_front_handler(&Session::OnHandshake, shared_from_this()));
}

void Session::OnHandshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "SSL Handshake error: " << ec.message() << std::endl;
        return;
    }
    // 完成 WebSocket 握手
    ws_.async_accept(
        beast::bind_front_handler(&Session::OnAccept, shared_from_this()));
}

void Session::OnAccept(beast::error_code ec) {
    if (ec) {
        std::cerr << "Accept error: " << ec.message() << std::endl;
        return;
    }
    ws_.control_callback([self = shared_from_this()](websocket::frame_type type,
                                                     beast::string_view) {
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

void Session::OnRead(beast::error_code ec, std::size_t bytes) {
    if (ec) {
        if (ec == websocket::error::closed) {
            SessionManager::GetInstance().Remove(user_id_);
        }
        Close();
        return;
    }

    try {
        // 记录原始消息
        std::string raw = beast::buffers_to_string(buffer_.data());
        Logger::instance().network_log(
            Logger::IN, socket.remote_endpoint().address().to_string(), raw,
            beast::buffers_to_string(buffer_.data()));

        auto msg = Message::FromJson(beast::buffers_to_string(buffer_.data()));
        HandleMessage(msg);
    } catch (const std::exception &e) {
        std::cerr << "Message parse error: " << e.what() << std::endl;
        Close();
        return;
    }

    buffer_.consume(bytes);
    DoRead();
}

void Session::HandleMessage(const Message &msg) {
    switch (msg.type) {
    case MsgType::Heartbeat:
        heartbeat_timer_.cancel();
        StartHeartbeatTimer();
        break;
    case MsgType::Login:
        // 实现认证逻辑
        user_id_ = msg.sender;
        SessionManager::GetInstance().Add(shared_from_this(), user_id_);
        break;
    case MsgType::Text:
        if (msg.receiver == "broadcast") {
            SessionManager::GetInstance().Broadcast(msg);
        } else {
            std::cout << msg.content << std::endl;
            SessionManager::GetInstance().SendToUser(msg.receiver, msg);
        }
        break;
    }
}

void Session::Send(const Message &msg) {
    // 将消息转换为json格式
    std::string msg_json = msg.ToJson();
    ws_.async_write(asio::buffer(msg_json),
                    [self = shared_from_this()](beast::error_code ec, size_t) {
                        if (ec)
                            self->Close();
                    });
    // 记录发送数据
    Logger::instance().network_log(
        Logger::IN, socket.remote_endpoint().address().to_string(), msg_json,
        beast::buffers_to_string(buffer_.data()));
}

void Session::StartHeartbeatTimer() {
    heartbeat_timer_.expires_after(std::chrono::seconds(30));
    heartbeat_timer_.async_wait(beast::bind_front_handler(
        &Session::OnHeartbeatTimer, shared_from_this()));
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
    ws_.async_ping(
        beast::websocket::ping_data{},
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
    ws_.next_layer().shutdown(ec);
    ws_.close(boost::beast::websocket::close_code::normal, ec);
    if (ec) {
        std::cerr << "Close error: " << ec.message() << std::endl;
    }
    heartbeat_timer_.cancel();
    pong_timeout_timer_.cancel();
}

} // namespace IM
