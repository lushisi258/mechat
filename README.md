# 即时通讯 MeChat

## 项目文件结构

```txt
im-server/
│── CMakeLists.txt          # CMake 构建配置
│── src/
│   │── main.cpp            # 入口文件，启动 WebSocket 服务器
│   │── server.cpp          # 服务器核心逻辑
│   │── server.hpp          # 服务器类声明
│   │── session.cpp         # WebSocket 会话管理
│   │── session.hpp         # WebSocket 会话类声明
│   │── message_handler.cpp # 处理消息逻辑
│   │── message_handler.hpp # 消息处理类声明
│   │── user_manager.cpp    # 管理用户连接状态
│   │── user_manager.hpp    # 用户管理类声明
│── include/                # 头文件目录
│   │── server.hpp
│   │── session.hpp
│   │── message_handler.hpp
│   │── user_manager.hpp
│── config/
│   │── server_config.json  # 配置文件，存储端口号等
│── tests/
│   │── test_server.cpp     # 测试 WebSocket 服务器
│   │── test_session.cpp    # 测试 WebSocket 会话
│── third_party/            # 第三方库（如 Boost）
│── logs/                   # 服务器日志文件存放目录
│── build/                  # 编译输出目录
│── README.md               # 项目说明文档
```

## 服务器架构

### 主要模块

1. 连接管理模块（Connection Manager）
维护在线用户会话，支持长连接心跳检测。
采用 IO多路复用（如 `boost::asio::io_context`）处理并发连接。

    *实现情况：已实现*

2. 消息路由模块（Message Router）
负责私聊、群聊消息的分发，支持点对点通信（P2P）和广播机制。
采用 发布/订阅模式（Pub-Sub） 实现消息推送，提高可扩展性。

    实现情况：未实现

3. 用户管理模块（User Manager）
处理用户注册、登录、身份认证（OAuth/JWT）。
维护用户在线状态，支持离线消息存储。

    实现情况：未实现

4. 存储模块（Storage Module）
Redis 缓存：存储在线用户、未读消息、聊天室成员信息。
数据库（MySQL/PostgreSQL）：存储用户信息、聊天记录、群组信息。

    实现情况：未实现

5. 加密与安全模块（Security Module）
使用 TLS/SSL 进行加密通信（Boost.Beast 支持）。
消息数据支持 AES/RSA 加密，防止中间人攻击。
采用令牌认证（JWT 或 OAuth） 进行用户权限验证。

    实现情况：未实现

6. 任务队列（Task Queue）
采用 Boost.Asio 线程池 或 Boost.Fiber 进行任务调度，提高性能。
处理离线消息推送、延迟任务等。

    实现情况：未实现

7. 日志与监控（Logging & Monitoring）
采用 Boost.Log 记录系统运行状态。
结合 Prometheus/Grafana 进行性能监控，优化吞吐量。

    实现情况：未实现

### 并发模型

IO 线程池 + 任务队列 处理消息读写，避免阻塞。
分布式负载均衡（如 Nginx + 反向代理） 保障大规模并发能力。
C++ 线程安全机制（如 std::mutex，boost::shared_mutex） 确保数据一致性。

实现情况：未实现

### 其他扩展

1. 支持离线消息推送（存储至 Redis 或数据库）

    实现情况：未实现

2. 消息持久化（MySQL + Kafka 异步存储）

    实现情况：未实现

3. 分布式部署（采用 Docker/Kubernetes 进行微服务化）

    实现情况：未实现
