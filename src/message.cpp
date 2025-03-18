// message.cpp
#include "message.hpp"

namespace IM {

void from_json(Message &msg, const json &j) {
    // 基础字段
    msg.message_id = j.value("message_id", "");
    // 解析 type 字段
    if (j.contains("type")) {
        if (j["type"].is_number()) {
            // 兼容旧版数值类型
            msg.type = static_cast<MsgType>(j["type"].get<int>());
        } else if (j["type"].is_string()) {
            std::string typeStr = j["type"].get<std::string>();
            msg.type = parseMsgType(typeStr);
        }
    }
    msg.receiver_id = j.value("receiver_id", "");
    msg.timestamp = j.value("timestamp", 0);

    // 解析sender
    try {
        if (j.contains("sender")) {
            const auto &s = j["sender"];
            msg.sender.user_id = s.value("user_id", "");
            msg.sender.username = s.value("username", "");
            msg.sender.avatar = s.value("avatar", "");
        }
    } catch (const json::parse_error &e) {
        std::cerr << "sender JSON 解析失败: " << e.what() << std::endl;
    }

    // 解析content
    try {
        if (j.contains("content")) {
            const auto &c = j["content"];
            // 解析 content.type 字段
            if (j.contains("content") && j["content"].contains("type")) {
                const auto &contentJson = j["content"];
                if (contentJson["type"].is_string()) {
                    std::string contentTypeStr =
                        contentJson["type"].get<std::string>();
                    msg.content.type = parseContentType(contentTypeStr);
                } else if (contentJson["type"].is_number()) {
                    msg.content.type = static_cast<ContentType>(
                        contentJson["type"].get<int>());
                }
            }
            msg.content.data = c.value("data", json::object());
        }
    } catch (const json::parse_error &e) {
        std::cerr << "sender JSON 解析失败: " << e.what() << std::endl;
    }

    // 解析metadata
    try {
        if (j.contains("metadata")) {
            const auto &m = j["metadata"];
            msg.metadata.status = m.value("status", "");
            msg.metadata.reply_to = m.value("reply_to", "");
            msg.metadata.is_encrypted = m.value("is_encrypted", false);
        }
    } catch (const json::parse_error &e) {
        std::cerr << "sender JSON 解析失败: " << e.what() << std::endl;
    }

    // 解析jwt_token
    if (j.contains("jwt_token")) {
        msg.jwt_token = j["jwt_token"].get<std::string>();
    }
}

json to_json(const Message &msg) {
    json j;

    j["message_id"] = msg.message_id;
    j["type"] = msg.type;
    j["sender"] = {{"user_id", msg.sender.user_id},
                   {"username", msg.sender.username},
                   {"avatar", msg.sender.avatar}};
    j["receiver_id"] = msg.receiver_id;
    j["timestamp"] = msg.timestamp;

    j["content"] = {{"type", msg.content.type}, {"data", msg.content.data}};

    // 序列化metadata
    json meta;
    meta["status"] = msg.metadata.status;
    meta["reply_to"] = msg.metadata.reply_to;
    meta["is_encrypted"] = msg.metadata.is_encrypted;
    j["metadata"] = meta;

    if (msg.jwt_token) {
        j["jwt_token"] = *msg.jwt_token;
    }

    return j;
}

} // namespace IM