// session_manager.cpp
#include "../include/session_manager.hpp"
#include "../include/session.hpp"

namespace IM {

std::unique_ptr<SessionManager, SessionManager::Deleter>
    SessionManager::instance_;
std::once_flag SessionManager::init_flag_;

void SessionManager::initialize(const nlohmann::json &config) {
    std::call_once(init_flag_, [&config]() {
        instance_.reset(new SessionManager(config));
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

void SessionManager::add(const std::string &user_id,
                         const std::shared_ptr<Session> session) {
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

std::pair<std::string, std::string>
SessionManager::generate_jwt(const std::string &user_id) {
    std::string refresh_token = generate_refresh_jwt(user_id);
    std::string access_token = generate_access_jwt(user_id, refresh_token);
    return {refresh_token, access_token};
}

std::string SessionManager::generate_refresh_jwt(const std::string &user_id) {
    auto refresh_token =
        jwt::create()
            .set_issuer("auth0")
            .set_type("JWS")
            .set_payload_claim("user_id", jwt::claim(user_id))
            .set_expires_at(std::chrono::system_clock::now() +
                            std::chrono::hours(360))
            .sign(jwt::algorithm::hs256{config_["jwt"]["refresh_secret_key"]});

    return refresh_token;
}

std::string SessionManager::generate_access_jwt(const std::string &user_id,
                                                const std::string &token) {
    if (validate_jwt(user_id, token, false))

    {
        auto access_token =
            jwt::create()
                .set_issuer("auth0")
                .set_type("JWS")
                .set_payload_claim("user_id", jwt::claim(user_id))
                .set_expires_at(std::chrono::system_clock::now() +
                                std::chrono::minutes(15))
                .sign(jwt::algorithm::hs256{config_["jwt"]["secret_key"]});

        return access_token;
    }

    else {
        return "";
    }
}

bool SessionManager::validate_jwt(const std::string &user_id,
                                  const std::string &token,
                                  bool is_access_token) {
    try {
        // 根据 token 类型的不同选择不同的 key
        std::string key;
        if (is_access_token) {
            key = config_["jwt"]["secret_key"];
        } else {
            key = config_["jwt"]["refresh_secret_key"];
        }
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs256{key})
                            .with_issuer("auth0");

        verifier.verify(decoded);

        if (user_id == decoded.get_payload_claim("user_id").as_string()) {
            return true;
        }
        return false;
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