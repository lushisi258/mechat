// message.cpp
#include "../include/message.hpp"
#include <chrono>

using json = nlohmann::json;

namespace IM {

Message Message::FromJson(const std::string &json_str) {
    auto j = json::parse(json_str);
    return {static_cast<MsgType>(j["type"].get<int>()),
            j["sender"],
            j["receiver"],
            j["timestamp"],
            j["content"],
            j["meta"]};
}

std::string Message::ToJson() const {
    json j;
    j["type"] = static_cast<int>(type);
    j["sender"] = sender;
    j["receiver"] = receiver;
    j["timestamp"] = timestamp;
    j["content"] = content;
    j["meta"] = meta;
    return j.dump();
}
} // namespace IM