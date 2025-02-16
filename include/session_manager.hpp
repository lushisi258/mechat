// session_manager.hpp
#pragma once
#include "message.hpp"
#include "session.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace IM {

class SessionManager {
  public:
    static SessionManager &GetInstance();

    void Add(std::shared_ptr<Session> session, const std::string &user_id);
    void Remove(const std::string &user_id);
    void SendToUser(const std::string &user_id, const Message &msg);
    void Broadcast(const Message &msg);

  private:
    SessionManager() = default;
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
};

} // namespace IM