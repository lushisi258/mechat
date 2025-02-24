// session_manager.hpp
#pragma once
#include "message.hpp"
#include <jwt-cpp/jwt.h>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unordered_map>

namespace IM {

class Session;

class SessionManager {
  public:
    // 初始化方法
    static void initialize(const nlohmann::json &config);
    static SessionManager &get_instance();

    SessionManager(const SessionManager &) = delete;
    SessionManager &operator=(const SessionManager &) = delete;

    void add(std::shared_ptr<Session> session, int user_id);
    void remove(int user_id);
    void send_to_user(int user_id, const Message &msg);
    void broadcast(const Message &msg);
    std::string generate_jwt(const std::string &user);
    bool validate_jwt(const std::string &token);
    void shutdown();

  private:
    // 自定义删除器
    struct Deleter {
        void operator()(SessionManager *p) const {
            delete p; // 可以访问私有析构
        }
    };

    SessionManager(const nlohmann::json &config);
    ~SessionManager() = default; // 保持私有

    nlohmann::json config_;
    std::mutex mutex_;
    std::unordered_map<int, std::shared_ptr<Session>> sessions_;

    static std::unique_ptr<SessionManager, Deleter>
        instance_; // 使用带删除器的unique_ptr
    static std::once_flag init_flag_;
};

} // namespace IM