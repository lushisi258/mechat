// session_manager.cpp
#include "../include/session_manager.hpp"

namespace IM {

SessionManager &SessionManager::GetInstance() {
    static SessionManager instance;
    return instance;
}

void SessionManager::add(std::shared_ptr<Session> session,
                         const std::string &user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[user_id] = session;
}

void SessionManager::remove(const std::string &user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(user_id);
}

void SessionManager::send_to_user(const std::string &user_id,
                                const Message &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = sessions_.find(user_id); it != sessions_.end()) {
        it->second->send(msg);
    }
}

void SessionManager::broadcast(const Message &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &[uid, session] : sessions_) {
        session->send(msg);
    }
}
} // namespace IM