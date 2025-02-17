# MeChat API 文档

- 服务器 URL: `wss://pc.lushisi.top:2233`

## 纯文本消息

- 参数说明:
  - `type`: `type`为`1`代表是纯文本消息
  - `sender`: 发送者uid
  - `receiver`: 接收者uid
  - `timestamp`: 发送时间戳
  - `content`: 文本内容
  - `meta`: 附加内容
- 消息格式:

    ```json
    {
        "type": 1,
        "sender": "me",
        "receiver": "you",
        "timestamp": 121212,
        "content": "good morning",
        "meta": ""
    }
    ```
