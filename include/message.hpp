// message.hpp
#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace IM {
// 消息类型
enum class MsgType {
    Text = 1,        // 纯文本
    Image = 2,       // 图片
    Heartbeat = 3,   // 心跳
    Login = 4,       // 登录
    Register = 5,    // 注册
    StatusNotify = 6 // 状态
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
    static Message from_json(const std::string &json_str);
    // 将消息转化为json数据
    std::string to_json() const;
};
} // namespace IM