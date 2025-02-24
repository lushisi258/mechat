// session_manager.cpp
#include "../include/session_manager.hpp"
#include "../include/session.hpp"

namespace IM {

std::unique_ptr<SessionManager, SessionManager::Deleter>
    SessionManager::instance_;
std::once_flag SessionManager::init_flag_;

void SessionManager::initialize(const nlohmann::json &config) {
    std::call_once(init_flag_, [&config]() {
        instance_.reset(new SessionManager(config)); // 使用reset创建实例
    });
}

SessionManager &SessionManager::get_instance() {
    if (!instance_) {
        throw std::runtime_error(
            "SessionManager not initialized. Call initialize() first.");
    }
    return *instance_;
}

SessionManager::SessionManager(const nlohmann::json &config)
    : config_(config) {}

void SessionManager::add(std::shared_ptr<Session> session, int user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[user_id] = session;
}

void SessionManager::remove(const int user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(user_id);
}

void SessionManager::send_to_user(const int user_id, const Message &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (auto it = sessions_.find(user_id); it != sessions_.end()) {
        it->second->send(msg);
    }
}

std::string SessionManager::generate_jwt(const std::string &user) {
    auto token = jwt::create()
                     .set_issuer("auth0")
                     .set_type("JWS")
                     .set_payload_claim("user", jwt::claim(user))
                     .sign(jwt::algorithm::hs256{config_["jwt"]["secret_key"]});
    return token;
}

bool SessionManager::validate_jwt(const std::string &token) {
    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs256{
                                config_["jwt"]["secret_key"]})
                            .with_issuer("auth0");

        verifier.verify(decoded);
        return true;
    } catch (const std::exception &e) {
        std::cerr << "JWT validation failed: " << e.what() << std::endl;
        return false;
    }
}

void SessionManager::broadcast(const Message &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &[uid, session] : sessions_) {
        session->send(msg);
    }
}

void SessionManager::shutdown() {}
} // namespace IM