#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <iostream>
#include <memory>
#include <string>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

void load_server_certificate(ssl::context &ctx) {
    ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 |
                    ssl::context::no_sslv3 | ssl::context::single_dh_use);

    ctx.use_certificate_chain_file(
        "/etc/letsencrypt/live/pc.lushisi.top/fullchain.pem");
    ctx.use_private_key_file("/etc/letsencrypt/live/pc.lushisi.top/privkey.pem",
                             ssl::context::pem);
}

class wss_session : public std::enable_shared_from_this<wss_session> {
    websocket::stream<boost::asio::ssl::stream<tcp::socket>> ws_;
    beast::flat_buffer buffer_;

  public:
    explicit wss_session(tcp::socket &&socket, ssl::context &ctx)
        : ws_(std::move(socket), ctx) {}

    void run() {
        auto self = shared_from_this();
        ws_.next_layer().async_handshake(
            ssl::stream_base::server, [this, self](beast::error_code ec) {
                if (ec)
                    return fail(ec, "SSL handshake");
                do_ws_handshake();
            });
    }

  private:
    void do_ws_handshake() {
        auto self = shared_from_this();
        ws_.async_accept([this, self](beast::error_code ec) {
            if (ec)
                return fail(ec, "WebSocket handshake");
            do_read();
        });
    }

    void do_read() {
        auto self = shared_from_this();
        ws_.async_read(buffer_, [this, self](beast::error_code ec,
                                             std::size_t bytes_transferred) {
            if (ec)
                return fail(ec, "read");
            std::string msg = beast::buffers_to_string(buffer_.data());
            std::cout << "Received: " << msg << std::endl;
            buffer_.consume(bytes_transferred);
            do_write(msg);
        });
    }

    void do_write(std::string msg) {
        auto self = shared_from_this();
        ws_.async_write(asio::buffer(msg),
                        [this, self](beast::error_code ec, std::size_t) {
                            if (ec)
                                return fail(ec, "write");
                            do_read();
                        });
    }

    void fail(beast::error_code ec, char const *what) {
        std::cerr << what << ": " << ec.message() << std::endl;
    }
};

class wss_server {
    asio::io_context &ioc_;
    ssl::context ctx_{ssl::context::tlsv12};
    tcp::acceptor acceptor_;

  public:
    wss_server(asio::io_context &ioc, short port)
        : ioc_(ioc), acceptor_(ioc, tcp::endpoint(tcp::v6(), port)) {
        load_server_certificate(ctx_);
        accept();
    }

  private:
    void accept() {
        acceptor_.async_accept([this](beast::error_code ec,
                                      tcp::socket socket) {
            if (!ec)
                std::make_shared<wss_session>(std::move(socket), ctx_)->run();
            accept();
        });
    }
};

int main() {
    try {
        asio::io_context ioc;
        wss_server server(ioc, 2233);
        ioc.run();
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
