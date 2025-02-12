#ifndef SERVER_HPP
#define SERVER_HPP

#include <boost/asio.hpp>
#include <memory>

class Session;

class Server {
  public:
    Server(boost::asio::io_context &ioc, unsigned short port);
    void Start();

  private:
    void DoAccept();
    void HandleAccept(std::shared_ptr<Session> session,
                      const boost::system::error_code &error);

    boost::asio::io_context &io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
};

#endif // SERVER_HPP