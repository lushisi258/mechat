// message.cpp
#include "message.hpp"

namespace IM {

void from_json(Message &msg, const json &j) {
    // 基础字段
    msg.message_id = j.value("message_id", "");
    msg.type = j.value("type", MsgType::Data);
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
            msg.content.type = c.value("type", ContentType::Text);
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