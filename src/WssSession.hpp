#pragma once
#include "MsgDispatcher.hpp"
#include "message.pb.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <cstddef>
#include <memory>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace ssl = asio::ssl;

class MsgDispatcher;

// Manages an individual WebSocket session
class WssSession : public std::enable_shared_from_this<WssSession> {
  public:
    WssSession(asio::ip::tcp::socket socket, ssl::context &ctx,
               std::shared_ptr<MsgDispatcher> dispatcher);

    // Starts the session's asynchronous operations
    void run();

  private:
    // Callback for SSL handshake completion
    void on_handshake(beast::error_code ec);

    // Callback for WebSocket upgrade completion
    void on_accept(beast::error_code ec);

    // Initiates an asynchronous read operation
    void do_read();

    // Callback for read operation completion
    void on_read(beast::error_code ec, std::size_t bytes_transferred);

    websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
    std::shared_ptr<MsgDispatcher> dispatcher_;
    beast::flat_buffer buffer_;
};