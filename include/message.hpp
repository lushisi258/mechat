// message.hpp
#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace IM {
// 消息类型
enum class MsgType {
    Text = 1,           // 纯文本
    Image = 2,          // 图片
    Heartbeat = 3,      // 心跳
    Login = 4,          // 登录
    Register = 5,       // 注册
    Logout = 6,         // 注销
    StatusNotify = 7,   // 状态
    UpdateJWT = 8       // 更新token
};

// 消息结构
struct Message {
    MsgType type;
    std::string sender;
    std::string receiver;
    int64_t timestamp;
    std::string content;
    std::string meta;
    std::string token;

    // 将json数据转化为消息对象
    static Message from_json(const std::string &json_str);
    // 将消息转化为json数据
    std::string to_json() const;
};
} // namespace IM