// server.hpp
#pragma once
#include <boost/asio.hpp>
#include "session.hpp"

namespace IM {

class Server {
public:
    Server(asio::io_context& ioc, unsigned short port);
    void Start();

private:
    void DoAccept();
    void HandleAccept(std::shared_ptr<Session> session, const boost::system::error_code& error);

    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;
};

} // namespace IM