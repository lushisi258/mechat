# MeChat API 文档 (v2)

- 服务器 URL: `wss://pc.lushisi.top:2233`
- 消息协议版本：2024.1 (基于强类型消息结构)

---

## 协议层级结构

  ```json
  {
    "message_id": "uuidv4",
    "type": "消息主类型: data(1)/control(2)/system(3)",
    "sender": {
      "user_id": "用户唯一标识",
      "username": "显示名称",
      "avatar": "头像URL"
    },
    "receiver_id": "接收方ID",
    "timestamp": 1672531200000,
    "content": {
      "type": "内容子类型: text(1)/image(2)/file(3)/login(4)/...",
      "data": "具体内容结构"
    },
    "metadata": {
      "status": "消息状态",
      "reply_to": "回复消息ID",
      "is_encrypted": false
    },
    "jwt_token": "鉴权令牌"
  }
  ```

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
  - `token`: 返回消息中为 access token
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
        "sender": "user_nickname",
        "meta": "refresh token",
        "token": "access token",
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
        "meta": "refresh token",
        "token": "access token",
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

## 刷新 `access token`

- 参数说明:
  - `meta`: refresh token

- 发送消息：

  ```json
  {
    "sender": "user email",
    "meta": "refresh token",
    "timestamp": $timestamp
  }
  ```

- 返回消息：

  ```json
  {
    "token": "access token",
    "timestamp": $timestamp
  }
  ```
