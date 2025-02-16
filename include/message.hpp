// message.hpp
#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace IM {
// 消息类型
enum class MsgType {
    Text = 1,
    Image = 2,
    Heartbeat = 3,
    Login = 4,
    StatusNotify = 5
};

// 消息结构
struct Message {
    MsgType type;
    std::string sender;
    std::string receiver;
    int64_t timestamp;
    std::string content;
    std::string meta;

    // 将json数据转化为消息对象
    static Message FromJson(const std::string& json_str);
    // 将消息转化为json数据
    std::string ToJson() const;
};
} // namespace IM