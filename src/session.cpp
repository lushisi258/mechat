// session.cpp
#include "../include/session.hpp"
#include "../include/session_manager.hpp"

namespace IM {

Session::Session(asio::io_context &ioc, asio::ssl::context &ssl_context,
                 MySQLConnectionPool &mysql_pool,
                 RedisConnectionPool &redis_pool,
                 MongoDBConnectionPool &mongo_pool)
    : ws_(asio::make_strand(ioc), ssl_context),
      heartbeat_timer_(ws_.get_executor()),
      pong_timeout_timer_(ws_.get_executor()), mysql_pool_(mysql_pool),
      redis_pool_(redis_pool), mongo_pool_(mongo_pool) {}

tcp::socket &Session::socket() { return ws_.next_layer().next_layer(); }

void Session::start() {
    // SSL 握手
    do_handshake();
}

void Session::do_handshake() {
    // 启动 SSL 握手
    ws_.next_layer().async_handshake(
        boost::asio::ssl::stream_base::server,
        beast::bind_front_handler(&Session::on_handshake, shared_from_this()));
}

void Session::on_handshake(beast::error_code ec) {
    if (ec) {
        std::cerr << "SSL Handshake error: " << ec.message() << std::endl;
        return;
    }
    // 完成 WebSocket 握手
    ws_.async_accept(
        beast::bind_front_handler(&Session::on_accept, shared_from_this()));
}

void Session::on_accept(beast::error_code ec) {
    if (ec) {
        std::cerr << "Accept error: " << ec.message() << std::endl;
        return;
    }
    ws_.control_callback([self = shared_from_this()](websocket::frame_type type,
                                                     beast::string_view) {
        if (type == websocket::frame_type::pong) {
            self->on_pong_received();
        }
    });
    start_heartbeat_timer();
    do_read();
}

void Session::do_read() {
    ws_.async_read(buffer_, beast::bind_front_handler(&Session::on_read,
                                                      shared_from_this()));
}

void Session::on_read(beast::error_code ec, std::size_t bytes) {
    if (ec) {
        if (ec == websocket::error::closed) {
            SessionManager::get_instance().remove(user_id_);
        }
        close();
        return;
    }

    try {
        // 记录原始消息
        std::string raw = beast::buffers_to_string(buffer_.data());
        Logger::instance().network_log(
            Logger::IN, socket().remote_endpoint().address().to_string(), raw,
            beast::buffers_to_string(buffer_.data()));

        // 读取缓冲区的json并转化为消息类型
        std::string response = beast::buffers_to_string(buffer_.data());
        json j = json::parse(response);
        Message msg;
        from_json(msg, j);

        handle_message(msg);

    } catch (const std::exception &e) {
        std::cerr << "Message parse error: " << e.what() << std::endl;
        close();
        return;
    }

    buffer_.consume(bytes);
    do_read();
}

void Session::handle_message(const Message &msg) {
    // 打印消息
    std::cout << "收到消息内容: " << msg.content.data.dump() << std::endl;

    switch (msg.type) { // 主类型判断

    case MsgType::Data:             // 数据消息分支
        switch (msg.content.type) { // 内容子类型判断
        case ContentType::Text:
            if (msg.receiver_id == "broadcast") { // 修正字段名为receiver_id
                SessionManager::get_instance().broadcast(msg);
            } else if (!user_id_.empty()) {
                SessionManager::get_instance().send_to_user(msg.receiver_id,
                                                            msg);
            }
            break;
        case ContentType::Image:
            // 处理图片消息
            // handle_image_message(msg);
            break;
        case ContentType::File:
            // 文件处理
            // handle_file_transfer(msg);
            break;
        default:
            std::cerr << "未知的数据消息类型: "
                      << static_cast<int>(msg.content.type) << std::endl;
        }
        break;

    case MsgType::Control: // 控制消息分支
        switch (msg.content.type) {
        case ContentType::Heartbeat:
            // 心跳处理保持原有逻辑
            heartbeat_timer_.cancel();
            start_heartbeat_timer();
            break;
        case ContentType::Login:
            // 带JWT的认证逻辑
            if (msg.jwt_token.has_value()) {
                recv_login_msg(msg);
            } else {
                // send_error("缺少认证令牌");
            }
            break;
        default:
            std::cerr << "未知的控制消息类型: "
                      << static_cast<int>(msg.content.type) << std::endl;
        }
        break;

    case MsgType::System: // 系统消息分支
        // 系统状态通知处理
        if (msg.metadata.status == "emergency") {
            // handle_emergency_notification(msg);
        }
        break;

    default:
        // 错误处理增加日志细节
        std::cerr << "非法主消息类型: " << static_cast<int>(msg.type)
                  << " 消息ID: " << msg.message_id << std::endl;
        // send_error("非法消息类型", msg.message_id);
    }
}

int Session::recv_register_msg(const Message &msg) {
    // 1. 参数有效性检查
    if (!msg.content.data.contains("email") ||
        !msg.content.data.contains("password")) {
        // send_error("注册信息不完整", msg.message_id);
        return EXIT_FAILURE;
    }

    // 2. 从content.data获取注册信息
    std::string email = msg.content.data["email"].get<std::string>();
    std::string password = msg.content.data["password"].get<std::string>();

    // 3. 密码哈希处理
    std::string password_hash = do_hash(password);

    // 4. 数据库操作
    auto conn = mysql_pool_.get_connection();
    constexpr std::string_view query =
        "INSERT INTO users (email, password_hash) VALUES (?, ?)";

    // 5. 使用RAII管理语句句柄
    MYSQL_STMT *stmt = mysql_stmt_init(conn.get());
    if (!stmt)
        return EXIT_FAILURE;

    auto stmt_guard = std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)>(
        stmt, mysql_stmt_close);

    // 6. 准备语句
    if (mysql_stmt_prepare(stmt, query.data(), query.size())) {
        // log_error("Prepare error: ", mysql_stmt_error(stmt));
        return EXIT_FAILURE;
    }

    // 7. 参数绑定
    MYSQL_BIND params[2] = {};

    // Email参数
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = email.data();
    params[0].buffer_length = email.length();

    // 密码哈希参数
    params[1].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer = password_hash.data();
    params[1].buffer_length = password_hash.length();

    if (mysql_stmt_bind_param(stmt, params)) {
        // log_error("Bind error: ", mysql_stmt_error(stmt));
        return EXIT_FAILURE;
    }

    // 8. 执行插入
    if (mysql_stmt_execute(stmt)) {
        const auto err_no = mysql_errno(conn.get());
        if (err_no == 1062) { // Duplicate entry
            // send_error("邮箱已存在", msg.message_id);
        } else {
            // log_error("Execute error: ", mysql_stmt_error(stmt));
        }
        return EXIT_FAILURE;
    }

    // 9. 获取用户ID
    user_id_ = std::to_string(mysql_insert_id(conn.get()));
    if (user_id_.empty()) {
        // send_error("注册信息保存失败", msg.message_id);
        return EXIT_FAILURE;
    }

    // 10. 会话管理
    SessionManager::get_instance().add(user_id_, shared_from_this());

    // 11. 构造响应消息
    Message response_msg{};
    response_msg.message_id = generate_uuid(); // 需要实现UUID生成
    response_msg.type = MsgType::Control;
    response_msg.content.type = ContentType::Login;
    response_msg.timestamp = generate_timestamp();

    // 12. 生成令牌
    auto [access_token, refresh_token] =
        SessionManager::get_instance().generate_jwt(email);

    // 13. 设置元数据
    response_msg.metadata.status = "success";
    response_msg.metadata.reply_to = msg.message_id;

    // 14. 设置认证令牌
    response_msg.jwt_token = access_token;

    // 15. 发送响应
    send(response_msg);

    return EXIT_SUCCESS;
}

int Session::recv_login_msg(const Message &msg) {
    if (!msg.content.data.contains("email") ||
        !msg.content.data.contains("password")) {
        // send_error("登录信息不完整", msg.message_id);
        return EXIT_FAILURE;
    }
    // 处理密码
    std::string password_hash = do_hash(msg.content.data["password"]);

    // 获取连接，构造查询语句
    auto conn = mysql_pool_.get_connection();
    std::string query = "SELECT id, nickname "
                        " FROM users "
                        " WHERE email = ? AND password_hash = ? "
                        " LIMIT 1";

    // 初始化MYSQL模型
    MYSQL_STMT *stmt = mysql_stmt_init(conn.get());
    if (!stmt)
        return EXIT_FAILURE;

    // 创建锁，预处理查询
    auto stmt_guard = std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)>(
        stmt, mysql_stmt_close);
    if (mysql_stmt_prepare(stmt, query.c_str(), query.size())) {
        std::string error_info =
            "Prepare error: " + std::string(mysql_stmt_error(stmt));
        Logger::instance().bug_log(error_info);
        return EXIT_FAILURE;
    }

    // 参数绑定
    std::string email = msg.content.data["email"];
    MYSQL_BIND params[2] = {};
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = email.data();
    params[0].buffer_length = email.size();

    params[1].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer = password_hash.data();
    params[1].buffer_length = password_hash.size();

    if (mysql_stmt_bind_param(stmt, params)) {
        std::string error_info =
            "Bind error: " + std::string(mysql_stmt_error(stmt));
        Logger::instance().bug_log(error_info);
        return EXIT_FAILURE;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        std::string error_info =
            "Execute error: " + std::string(mysql_stmt_error(stmt));
        Logger::instance().bug_log(error_info);
        return EXIT_FAILURE;
    }

    // 获取结果
    std::string nickname;

    MYSQL_BIND result[2] = {};
    result[0].buffer_type = MYSQL_TYPE_BIT;
    result[0].buffer = &user_id_;

    result[1].buffer_type = MYSQL_TYPE_STRING;
    result[1].buffer = reinterpret_cast<char *>(&nickname);
    result[1].buffer_length = nickname.capacity();

    if (mysql_stmt_bind_result(stmt, result)) {
        std::string error_info =
            "Bind error: " + std::string(mysql_stmt_error(stmt));
        Logger::instance().bug_log(error_info);
        return EXIT_FAILURE;
    }

    if (mysql_stmt_store_result(stmt)) {
        std::string error_info =
            "Store error: " + std::string(mysql_stmt_error(stmt));
        Logger::instance().bug_log(error_info);
        return EXIT_FAILURE;
    }

    if (mysql_stmt_fetch(stmt)) {
        std::string error_info = "No matching user found";
        Logger::instance().log(Logger::INFO, error_info);
        return EXIT_FAILURE;
    }

    std::string login_info = "User: " + email + " Login success";
    Logger::instance().log(Logger::INFO, login_info);

    // 会话管理
    SessionManager::get_instance().add(user_id_, shared_from_this());

    // 发送响应
    Message response_msg{};
    response_msg.message_id = generate_uuid();      // 生成唯一消息ID
    response_msg.type = MsgType::Control;           // 主类型为控制消息
    response_msg.content.type = ContentType::Login; // 子类型为登录

    // 设置发送者信息（根据新UserInfo结构）
    response_msg.sender = UserInfo{
        .user_id = user_id_,  // 从会话获取用户ID
        .username = nickname, // 使用传入的昵称
        .avatar = ""          // 默认空头像
    };

    // 设置时间戳
    response_msg.timestamp = generate_timestamp();

    // 生成JWT令牌对
    auto [access_token, refresh_token] =
        SessionManager::get_instance().generate_jwt(user_id_);
    // 访问令牌
    response_msg.jwt_token = access_token;
    // 刷新令牌
    response_msg.content.data["refresh_token"] = refresh_token;

    // 设置元数据
    response_msg.metadata = {
        .status = "auth_success",   // 认证状态
        .reply_to = msg.message_id, // 关联原始消息ID
        .is_encrypted = false       // 默认不加密
    };

    // 在content.data中添加补充信息
    response_msg.content.data = {
        {"user_info",
         {{"user_id", response_msg.sender.user_id}, {"nickname", nickname}}},
        {"token_expire", 3600} // 令牌有效期示例
    };

    send(response_msg);

    return EXIT_SUCCESS;
}

int Session::recv_fresh_access_token_msg(const Message &msg) {
    // std::string access_token;
    // try {
    //     access_token = SessionManager::get_instance().generate_access_jwt(
    //         msg.sender, msg.meta);
    // } catch (const std::exception &e) {
    //     std::string error_info =
    //         "Fresh access token failed: " + std::string(e.what());
    //     Logger::instance().log(Logger::ERROR, error_info);
    //     return EXIT_FAILURE;
    // }

    // Message response_msg{};

    // response_msg.type = MsgType::UpdateJWT;
    // response_msg.timestamp =
    //     std::chrono::system_clock::now().time_since_epoch().count();
    // response_msg.token = access_token;

    // send(response_msg);

    return EXIT_SUCCESS;
}

void Session::send(const Message &msg) {
    // 将消息转换为json格式
    std::string msg_json = to_json(msg);
    ws_.async_write(asio::buffer(msg_json),
                    [self = shared_from_this()](beast::error_code ec, size_t) {
                        if (ec)
                            self->close();
                    });
    // 记录发送数据
    Logger::instance().network_log(
        Logger::OUT, socket().remote_endpoint().address().to_string(), msg_json,
        beast::buffers_to_string(buffer_.data()));
}

void Session::start_heartbeat_timer() {
    heartbeat_timer_.expires_after(std::chrono::seconds(30));
    heartbeat_timer_.async_wait(beast::bind_front_handler(
        &Session::on_heartbeat_timer, shared_from_this()));
}

void Session::on_heartbeat_timer(beast::error_code ec) {
    if (ec == asio::error::operation_aborted) {
        // 定时器被取消，忽略
        return;
    }
    if (ec) {
        std::cerr << "Heartbeat timer error: " << ec.message() << std::endl;
        return;
    }

    // 发送Ping
    ws_.async_ping(
        beast::websocket::ping_data{},
        beast::bind_front_handler(&Session::on_ping_sent, shared_from_this()));
}

void Session::on_ping_sent(beast::error_code ec) {
    if (ec) {
        std::cerr << "Ping send error: " << ec.message() << std::endl;
        close();
        return;
    }

    // 启动Pong超时定时器
    start_pong_timeout_timer();
}

void Session::start_pong_timeout_timer() {
    pong_timeout_timer_.expires_after(std::chrono::seconds(5));
    pong_timeout_timer_.async_wait(beast::bind_front_handler(
        &Session::on_pong_timeout, shared_from_this()));
}

void Session::on_pong_timeout(beast::error_code ec) {
    if (ec == asio::error::operation_aborted) {
        // 定时器被取消，忽略
        return;
    }
    if (ec) {
        std::cerr << "Pong timeout timer error: " << ec.message() << std::endl;
        return;
    }

    std::cerr << "Pong timeout. Closing connection." << std::endl;
    close();
}

void Session::on_pong_received() {
    // 取消Pong超时定时器
    pong_timeout_timer_.cancel();

    // 重新启动心跳定时器
    heartbeat_timer_.cancel();
    start_heartbeat_timer();
}

void Session::close() {
    beast::error_code ec;
    ws_.next_layer().shutdown(ec);
    ws_.close(boost::beast::websocket::close_code::normal, ec);
    if (ec) {
        std::cerr << "close error: " << ec.message() << std::endl;
    }
    heartbeat_timer_.cancel();
    pong_timeout_timer_.cancel();
}

std::string Session::do_hash(const std::string &password) {
    // 对密码进行哈希处理
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(password.c_str()),
           password.size(), hash);
    // 将哈希结果转换为十六进制字符串
    std::string password_hash;
    char buf[3];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        password_hash += buf;
    }
    return password_hash;
}

// 生成时间戳
int64_t Session::generate_timestamp() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
}

std::string Session::generate_uuid() {
    // 1. 获取时间戳和时钟序列
    const auto now = std::chrono::system_clock::now();
    const auto since_epoch = now.time_since_epoch();
    const uint64_t timestamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(since_epoch)
            .count();

    // 2. 生成时钟序列（基于senderid哈希）
    static std::unordered_map<std::string, uint16_t> clock_seq_map;
    uint16_t clock_seq = std::hash<std::string>{}(user_id_) % 0x3FFF;

    // 3. 生成节点ID（基于senderid）
    uint8_t node_id[6];
    std::copy_n(user_id_.begin(), std::min(6, (int)user_id_.size()), node_id);

    // 4. 组合成UUID v1
    std::stringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8)
       << ((timestamp >> 32) & 0xFFFFFFFF) << "-" << std::setw(4)
       << ((timestamp >> 16) & 0xFFFF) << "-" << std::setw(4)
       << ((timestamp & 0x0FFF) | 0x1000) << "-"                 // Version 1
       << std::setw(4) << ((clock_seq & 0x3FFF) | 0x8000) << "-" // Variant
       << std::setw(12) << std::hex << static_cast<int>(node_id[0])
       << static_cast<int>(node_id[1]) << static_cast<int>(node_id[2])
       << static_cast<int>(node_id[3]) << static_cast<int>(node_id[4])
       << static_cast<int>(node_id[5]);

    return ss.str();
}

} // namespace IM
