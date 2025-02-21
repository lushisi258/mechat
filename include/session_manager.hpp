// session_manager.hpp
#pragma once
#include "../include/message.hpp"
#include "../include/session.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace IM {

class SessionManager {
  public:
    static SessionManager &GetInstance();

    void add(std::shared_ptr<Session> session, const std::string &user_id);
    void remove(const std::string &user_id);
    void send_to_user(const std::string &user_id, const Message &msg);
    void broadcast(const Message &msg);

  private:
    SessionManager() = default;
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
};

} // namespace IM