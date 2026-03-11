#include "WssSession.hpp"
#include <iostream>

WssSession::WssSession(asio::ip::tcp::socket socket, ssl::context& ctx) : ws_(std::move(socket), ctx) {}

void WssSession::run(){
    // SSL async handshake
    ws_.next_layer().async_handshake(ssl::stream_base::server, beast::bind_front_handler(&WssSession::on_handshake, shared_from_this()));
}

void WssSession::on_handshake(beast::error_code ec) {
    if(ec) return;
    // upgrade HTTP to WS
    ws_.async_accept(beast::bind_front_handler(&WssSession::on_accept, shared_from_this()));
}

void WssSession::on_accept(beast::error_code ec) {
    if(ec) return;
    do_read();
}

void WssSession::do_read() {
    // read data to buffer
    ws_.async_read(buffer_, beast::bind_front_handler(&WssSession::on_read, shared_from_this()));
}

void WssSession::on_read(beast::error_code ec, std::size_t) {
    if(ec == websocket::error::closed) return;
    if(ec) return;

    // handle msg
    // send back
    ws_.text(ws_.got_text());
    ws_.async_write(buffer_.data(), beast::bind_front_handler(&WssSession::on_write, shared_from_this()));
}

void WssSession::on_write(beast::error_code ec, std::size_t) {
    if(ec) return;
    // clean buffer
    buffer_.consume(buffer_.size());
    // continue read
    do_read();
}
