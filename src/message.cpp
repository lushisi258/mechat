// message.cpp
#include "../include/message.hpp"
#include <chrono>

namespace IM {

Message Message::from_json(const std::string &json_str) {
    auto j = nlohmann::json::parse(json_str);

    // 使用默认值处理缺失的数据
    MsgType type = j.value("type", MsgType::Text);
    std::string sender = j.value("sender", "");
    std::string receiver = j.value("receiver", "");
    int64_t timestamp = j.value("timestamp", 0);
    std::string content = j.value("content", "");
    std::string meta = j.value("meta", "");
    std::string token = j.value("token", "");

    return {static_cast<MsgType>(type),
            sender,
            receiver,
            timestamp,
            content,
            meta,
            token};
}

std::string Message::to_json() const {
    nlohmann::json j;
    j["type"] = static_cast<int>(type);
    j["sender"] = sender;
    j["receiver"] = receiver;
    j["timestamp"] = timestamp;
    j["content"] = content;
    j["meta"] = meta;
    j["token"] = token;
    return j.dump();
}

} // namespace IM