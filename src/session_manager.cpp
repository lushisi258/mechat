// session_manager.cpp
#include "../include/session_manager.hpp"

namespace IM {

SessionManager &SessionManager::GetInstance() {
    static SessionManager instance;
    return instance;
}

void SessionManager::Add(std::shared_ptr<Session> session,
                         const std::string &user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[user_id] = session;
}

void SessionManager::Remove(const std::string &user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(user_id);
}

void SessionManager::SendToUser(const std::string &user_id,
                                const Message &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = sessions_.find(user_id); it != sessions_.end()) {
        it->second->Send(msg);
    }
}

void SessionManager::Broadcast(const Message &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &[uid, session] : sessions_) {
        session->Send(msg);
    }
}
} // namespace IM