#include "WssSession.hpp"
#include <iostream>

WssSession::WssSession(asio::ip::tcp::socket socket, ssl::context &ctx)
    : ws_(std::move(socket), ctx) {}

void WssSession::run() {
    // SSL async handshake
    ws_.next_layer().async_handshake(
        ssl::stream_base::server,
        beast::bind_front_handler(&WssSession::on_handshake,
                                  shared_from_this()));
}

void WssSession::on_handshake(beast::error_code ec) {
    if (ec)
        return;
    // upgrade HTTP to WS
    ws_.async_accept(
        beast::bind_front_handler(&WssSession::on_accept, shared_from_this()));
}

void WssSession::on_accept(beast::error_code ec) {
    if (ec)
        return;
    do_read();
}

void WssSession::do_read() {
    // read data to buffer
    ws_.async_read(buffer_, beast::bind_front_handler(&WssSession::on_read,
                                                      shared_from_this()));
}

void WssSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec == websocket::error::closed)
        return;
    if (ec)
        return;

    // trans binary data to string
    std::string data = beast::buffers_to_string(buffer_.data());
    // parse msg to GameMessage
    mechat::GameMessage income_msg;
    if (income_msg.ParseFromString(data)) {
        // handle msg by type
        if (income_msg.type_id() == mechat::TEST) {
            handle_test_msg(income_msg);
        }
    }

    // clear buffer and continue listen
    buffer_.consume(bytes_transferred);
}

void WssSession::on_write(beast::error_code ec, std::size_t bytes_transferred) {
    if (ec)
        return;
    // clean buffer
    buffer_.consume(buffer_.size());
    // continue read
    do_read();
}

void WssSession::handle_test_msg(const mechat::GameMessage &msg) {
    mechat::TestMessage test_msg;
    if (test_msg.ParseFromString(msg.data())) {
        std::cout << test_msg.content() << std::endl;
    }
}

void WssSession::send_msg(const mechat::GameMessage &msg) {
    msg.SerializeToString(&write_data_);

    ws_.async_write(
        asio::buffer(write_data_),
        beast::bind_front_handler(&WssSession::on_write, shared_from_this()));
}