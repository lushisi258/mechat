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
    Data = 1,    // 数据消息（文本/图片等）
    Control = 2, // 控制消息（心跳、登录等）
    System = 3   // 系统消息
};

// 内容子类型
enum class ContentType {
    Text = 1,
    Image = 2,
    File = 3,
    Login = 4,
    Heartbeat = 5
};

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

// 枚举类型JSON转换
NLOHMANN_JSON_SERIALIZE_ENUM(MsgType, {{MsgType::Data, "data"},
                                       {MsgType::Control, "control"},
                                       {MsgType::System, "system"}})

NLOHMANN_JSON_SERIALIZE_ENUM(ContentType,
                             {{ContentType::Text, "text"},
                              {ContentType::Image, "image"},
                              {ContentType::File, "file"},
                              {ContentType::Login, "login"},
                              {ContentType::Heartbeat, "heartbeat"}})

// JSON转换方法
void from_json(Message &msg, const json &j);
json to_json(const Message &msg);

} // namespace IM