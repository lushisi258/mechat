#include "WssSession.hpp"
#include "MsgDispatcher.hpp"
#include <iostream>

WssSession::WssSession(asio::ip::tcp::socket socket, ssl::context &ctx,
                       std::shared_ptr<MsgDispatcher> dispatcher)
    : ws_(std::move(socket), ctx), dispatcher_(dispatcher) {}

void WssSession::run() {
    // Perform asynchronous SSL handshake
    ws_.next_layer().async_handshake(
        ssl::stream_base::server,
        beast::bind_front_handler(&WssSession::on_handshake,
                                  shared_from_this()));
}

void WssSession::on_handshake(beast::error_code ec) {
    if (ec)
        return;
    // Upgrade HTTP to WebSocket
    ws_.async_accept(
        beast::bind_front_handler(&WssSession::on_accept, shared_from_this()));
}

void WssSession::on_accept(beast::error_code ec) {
    if (ec)
        return;
    do_read();
}

void WssSession::do_read() {
    // Read data into the buffer
    ws_.async_read(buffer_, beast::bind_front_handler(&WssSession::on_read,
                                                      shared_from_this()));
}

void WssSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec == websocket::error::closed)
        return;
    if (ec)
        return;

    // Convert binary data to string
    std::string data = beast::buffers_to_string(buffer_.data());
    std::cout << "wss session 1" << std::endl;
    // Callback dispatcher
    if (dispatcher_) {
        std::cout << "wss session 2" << std::endl;
        dispatcher_->dispatch(shared_from_this(), data);
    }

    // Clear up buffer
    buffer_.consume(buffer_.size());
    do_read();
}