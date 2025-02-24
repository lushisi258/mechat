# MeChat API 文档

- 服务器 URL: `wss://pc.lushisi.top:2233`

---

## 纯文本消息

- 参数说明:
  - `type`: `type`为`1`代表是纯文本消息
  - `sender`: 发送者uid
  - `receiver`: 接收者uid
  - `timestamp`: 发送时间戳
  - `content`: 文本内容
- 消息格式:

    ```json
    {
        "type": 1,
        "sender": $sender_uid,
        "receiver": $receiver_uid,
        "timestamp": $timestamp,
        "content": "Hello, world!"
    }
    ```

---

## 登录消息

- 参数说明:
  - `type`: `type`为`4`代表是登录消息
  - `sender`: 账号（email_addr）
  - `meta`: 密码
  - `timestamp`: 发送时间戳
- 消息格式:

    ```json
    {
        "type": 4,
        "sender": "sender_email_addr",
        "meta": "password",
        "timestamp": $timestamp
    }
    ```

- 返回消息格式:
  - 登录成功：
  
    ```json
    {
        "type": 4,
        "content": "Login success",
        "meta": "jwt_content",
        "timestamp": $timestamp
    }
    ```

  - 登录失败：

    ```json
    {
        "type": 4,
        "content": "Login failed",
        "timestamp": $timestamp
    }
    ```

---

## 注册消息

- 参数说明:
  - `type`: `type`为`5`代表是注册消息
  - `sender`: 注册者手机号码
  - `meta`: 注册密码
  - `timestamp`: 发送时间戳
- 消息格式:

    ```json
    {
        "type": 5,
        "sender": "sender_email_addr",
        "meta": "password",
        "timestamp": $timestamp
    }
    ```
  
- 返回消息格式：
  - 注册成功（同步登录）：

    ```json
        ```json
    {
        "type": 4,
        "content": "Login success",
        "meta": "jwt_content",
        "timestamp": $timestamp
    }
    ```

  - 注册失败：

    ```json
    {
        "type": 5,
        "content": "Register failed",
        "timestamp": $timestamp
    }
    ```

---
