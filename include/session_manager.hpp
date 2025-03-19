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

    // 会话管理
    void add(const std::string &user_id,
             const std::shared_ptr<Session> session);
    void remove(const std::string &user_id);
    void send_to_user(const std::string &user_id, const Message &msg);
    void broadcast(const Message &msg);

    // 传入的 user 为用户的邮箱地址（即 msg.sender 部分的数据）
    // 注册时生成 refresh token 和 access token
    std::pair<std::string, std::string> generate_jwt(const std::string &email);
    // 生成 refresh token
    std::string generate_refresh_jwt(const std::string &email);
    // 生成 access token
    std::string generate_access_jwt(const std::string &email,
                                    const std::string &token);
    // 验证 token 合法性
    bool validate_jwt(const std::string &email, const std::string &token, bool is_access_token);
    // 根据 refresh token 刷新 access token
    std::string refresh_access_token(const std::string &email,
                                     const std::string &refresh_token);

    void shutdown();

  private:
    // 自定义删除器
    struct Deleter {
        void operator()(SessionManager *p) const { delete p; }
    };

    SessionManager(const nlohmann::json &config);
    ~SessionManager() = default;

    nlohmann::json config_;
    std::mutex mutex_;
    // 存储会话的键值对
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;

    static std::unique_ptr<SessionManager, Deleter> instance_;
    static std::once_flag init_flag_;
};

} // namespace IM