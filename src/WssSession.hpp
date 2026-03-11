#pragma once
#include <boost/asio/ssl/context.hpp>
#include <boost/beast.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <cstddef>
#include <memory>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
namespace ssl = asio::ssl;

// handle every single session
class WssSession : public std::enable_shared_from_this<WssSession> {
    public:
        WssSession(asio::ip::tcp::socket socket, ssl::context& ctx);
        void run();

    private:
        // handle handshake
        void on_handshake(beast::error_code ec);
        // handle accept msg
        void on_accept(beast::error_code ec);
        // read msg
        void do_read();
        // handle msg
        void on_read(beast::error_code ec, std::size_t bytes_transferred);
        void on_write(beast::error_code ec, std::size_t bytes_transferred);
  
        websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
        beast::flat_buffer buffer_;
};
