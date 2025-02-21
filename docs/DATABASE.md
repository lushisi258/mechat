# 即时通讯系统数据库表设计

## MySQL数据库表

### 1. 用户表（`users`）

| 字段            | 类型          | 说明                   |
|-----------------|---------------|------------------------|
| `id`            | `BIGINT`      | 用户ID，自增，主键      |
| `email`         | `VARCHAR(255)` | 用户邮箱，唯一，不能为空 |
| `password_hash` | `VARCHAR(255)` | 加密存储的密码，不能为空 |
| `nickname`      | `VARCHAR(50)`  | 昵称，默认空字符串      |
| `avatar_url`    | `TEXT`         | 头像URL，默认空         |
| `created_at`    | `TIMESTAMP`    | 用户创建时间，默认当前时间 |
| `updated_at`    | `TIMESTAMP`    | 用户更新时间，自动更新 |
| `idx_email`     | `INDEX`        | 为`email`字段建立索引   |

---

### 2. 好友关系表（`friends`）

| 字段         | 类型          | 说明                        |
|--------------|---------------|-----------------------------|
| `id`         | `BIGINT`      | 关系ID，自增，主键           |
| `user_id`    | `BIGINT`      | 用户ID，外键，不能为空       |
| `friend_id`  | `BIGINT`      | 好友ID，外键，不能为空       |
| `status`     | `TINYINT`     | 好友关系状态，0：待确认，1：已是好友 |
| `created_at` | `TIMESTAMP`   | 添加好友时间，默认当前时间   |
| `unique_friendship` | `UNIQUE` | 约束用户与好友的唯一性       |

---

### 3. 用户状态表（`user_status`）

| 字段        | 类型                | 说明                       |
|-------------|---------------------|----------------------------|
| `user_id`   | `BIGINT`            | 用户ID，外键，主键         |
| `status`    | `ENUM`              | 用户状态，'online', 'offline', 'busy' |
| `last_seen` | `TIMESTAMP`         | 最后一次在线时间（可为空） |

---

### 4. 会话表（`conversations`）

| 字段         | 类型            | 说明                        |
|--------------|-----------------|-----------------------------|
| `id`         | `BIGINT`        | 会话ID，自增，主键           |
| `type`       | `ENUM`          | 会话类型，'single' 或 'group' |
| `created_at` | `TIMESTAMP`     | 会话创建时间，默认当前时间   |

---

### 5. 会话参与者表（`conversation_users`）

| 字段             | 类型          | 说明                        |
|------------------|---------------|-----------------------------|
| `conversation_id`| `BIGINT`      | 会话ID，外键，不能为空       |
| `user_id`        | `BIGINT`      | 用户ID，外键，不能为空       |

---

### 6. 消息表（`messages`）

| 字段             | 类型          | 说明                       |
|------------------|---------------|----------------------------|
| `id`             | `BIGINT`      | 消息ID，自增，主键          |
| `message_type`   | `TINYINT`     | 消息类型                   |
| `conversation_id`| `BIGINT`      | 会话ID，外键，不能为空      |
| `sender_id`      | `BIGINT`      | 发送者ID，外键，不能为空    |
| `content`        | `TEXT`        | 消息内容，不能为空          |
| `sent_at`        | `TIMESTAMP`   | 消息发送时间，默认当前时间  |

---

### 表关系概述

- **`users` 表**：存储所有用户的基本信息。
- **`friends` 表**：存储用户之间的好友关系。
- **`user_status` 表**：存储用户的在线状态。
- **`conversations` 表**：存储所有的会话记录，支持单聊和群聊。
- **`conversation_users` 表**：存储会话参与者，支持群聊成员管理。
- **`messages` 表**：存储所有消息记录。

---

## Redis数据库表

---

## MongoDB数据库表

---
