// message.hpp
#pragma once
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace IM {
using json = nlohmann::json;

// 消息主类型
enum class MsgType {
    Data = 1,    // 数据消息
    Control = 2, // 控制消息
    System = 3,  // 系统消息
};

// 内容子类型
enum class ContentType {
    // Date 类型
    Text = 1,  // 纯文本
    Image = 2, // 图像
    File = 3,  // 文件
    // Control 类型
    Register = 4,             // 注册
    Login = 5,                // 登录
    FreshToken = 6,           // 刷新 token
    Logout = 7,               // 登出
    FriendRequest = 8,        // 加好友
    ApproveFriendRequest = 9, // 同意加好友
    DeleteFriend = 10,        // 删除好友
    // System 类型
};

// MsgType 映射表
const std::unordered_map<std::string, MsgType> msgTypeMap = {
    {"Data", MsgType::Data},
    {"Control", MsgType::Control},
    {"System", MsgType::System}};

// ContentType 映射表
const std::unordered_map<std::string, ContentType> contentTypeMap = {
    {"Text", ContentType::Text},
    {"Image", ContentType::Image},
    {"File", ContentType::File},
    {"Register", ContentType::Register},
    {"Login", ContentType::Login},
    {"FreshToken", ContentType::FreshToken},
    {"Logout", ContentType::Logout},
    {"FriendRequest", ContentType::FriendRequest},
    {"ApproveFriendRequest", ContentType::ApproveFriendRequest},
    {"DeleteFriend", ContentType::DeleteFriend}};

// 用户信息部分
struct UserInfo {
    std::string user_id{};
    std::string username{};
    std::string avatar{};
};

// 内容存储结构
struct MessageContent {
    ContentType type = ContentType::Text;
    json data;

    static MessageContent Text(const std::string &text) {
        return {ContentType::Text, {{"text", text}}};
    }

    static MessageContent Image(const std::string &url, int width, int height) {
        return {ContentType::Image,
                {{"url", url}, {"width", width}, {"height", height}}};
    }
};

// 元数据扩展部分
struct Metadata {
    std::string status{};
    std::string reply_to{};
    bool is_encrypted = false;

    friend void to_json(json &j, const Metadata &m) {
        j = json{{"status", m.status},
                 {"reply_to", m.reply_to},
                 {"is_encrypted", m.is_encrypted}};
    }

    friend void from_json(const json &j, Metadata &m) {
        j.at("status").get_to(m.status);
        j.at("reply_to").get_to(m.reply_to);
        m.is_encrypted = j.value("is_encrypted", false);
    }
};

// 主消息结构
struct Message {
    // 基础字段
    std::string message_id{};     // 默认空字符串
    MsgType type = MsgType::Data; // 默认数据消息
    UserInfo sender{};            // 默认构造
    std::string receiver_id{};    // 默认空字符串
    int64_t timestamp = 0;        // 默认0时间戳

    // 内容部分
    MessageContent content{}; // 默认构造

    // 扩展元数据
    Metadata metadata{}; // 默认构造

    // 认证信息
    std::optional<std::string> jwt_token; // 默认nullopt
};

// JSON转换方法
void from_json(Message &msg, const json &j);
json to_json(const Message &msg);

inline MsgType parseMsgType(const std::string &typeStr) {
    auto it = msgTypeMap.find(typeStr);
    return (it != msgTypeMap.end()) ? it->second : MsgType::Data;
}

inline ContentType parseContentType(const std::string &typeStr) {
    auto it = contentTypeMap.find(typeStr);
    return (it != contentTypeMap.end()) ? it->second : ContentType::Text;
}

} // namespace IM