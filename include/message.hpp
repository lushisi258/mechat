// message.hpp
#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace IM {
enum class MsgType {
    Text = 1,
    Image = 2,
    Heartbeat = 3,
    Login = 4,
    StatusNotify = 5
};

struct Message {
    MsgType type;
    std::string sender;
    std::string receiver;
    int64_t timestamp;
    std::string content;
    std::string meta;

    static Message FromJson(const std::string& json_str);
    std::string ToJson() const;
};
} // namespace IM