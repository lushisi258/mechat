#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <memory>
#include <string>

namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

class MsgDispatcher;

// listen new connection and call session to handle
class WssServer : public std::enable_shared_from_this<WssServer> {
  public:
    WssServer(asio::io_context &ioc, unsigned short port,
              std::shared_ptr<MsgDispatcher> dispatcher);
    void run();

  private:
    void do_accept();
    void on_accept(boost::system::error_code ec, tcp::socket socket);

    asio::io_context &ioc_;
    tcp::acceptor acceptor_;
    ssl::context ctx_;
    std::shared_ptr<MsgDispatcher> dispatcher_;
};
