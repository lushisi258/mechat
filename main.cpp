#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = boost::asio::ip::tcp;

int main(){
    // network io context
    net::io_context ioc;

    // set ssl certificate
    ssl::context ctx{ssl::context::tlsv12};
    ctx.use_certificate_chain_file("/etc/letsencrypt/live/lushisi.top/fullchain.pem");
    ctx.use_private_key_file("/etc/letsencrypt/live/lushisi.top/privkey.pem", ssl::context::pem);

    // listen port 2233
    tcp::acceptor acceptor{ioc, {net::ip::make_address("::"), 2233}};

    // listen connect and print message
    while(true) {
        tcp::socket socket{ioc};
        acceptor.accept(socket);

        std::thread([s = std::move(socket), &ctx]() mutable {
                websocket::stream<beast::ssl_stream<tcp::socket>> ws {std::move(s), ctx};
                ws.next_layer().handshake(ssl::stream_base::server);
                ws.accept();

                while(true) {
                // read buffer and print
                beast::flat_buffer buffer;
                ws.read(buffer);
                ws.write(buffer.data());
                }
                }).detach();
    }
}
