#include "WssServer.hpp"
#include "WssSession.hpp"
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/system/error_code.hpp>

WssServer::WssServer(asio::io_context &ioc, unsigned short port,
                     std::shared_ptr<MsgDispatcher> dispatcher)
    : ioc_(ioc), acceptor_(ioc, {tcp::v6(), port}), ctx_(ssl::context::tlsv12),
      dispatcher_(dispatcher) {
    // load CA
    ctx_.use_certificate_chain_file(
        "/home/lushisi/projects/mechat/cert/fullchain.pem");
    ctx_.use_private_key_file("/home/lushisi/projects/mechat/cert/privkey.pem",
                              ssl::context::pem);
}

// run server
void WssServer::run() { do_accept(); }

// listen and handle connection
void WssServer::do_accept() {
    acceptor_.async_accept(
        asio::make_strand(ioc_),
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                // recv a new connection
                // then create a new session client to handle
                std::make_shared<WssSession>(std::move(socket), ctx_,
                                             dispatcher_)
                    ->run();
            }
            // wait for new connection
            do_accept();
        });
}
