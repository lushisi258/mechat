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
    // 不需要 access_token 的消息类型
    if (!msg.jwt_token.has_value()) {
        switch (msg.type) {
        // 控制类型的消息
        case MsgType::Control:
            switch (msg.content.type) {
            // 注册
            case ContentType::Register:
                recv_register_msg(msg);
                break;
            // 登录
            case ContentType::Login:
                recv_login_msg(msg);
                break;
            // 刷新 token
            case ContentType::FreshToken:
                recv_fresh_access_token_msg(msg);
                break;
            default:
                std::cerr << "no access_token 未知的控制消息类型: "
                          << static_cast<int>(msg.content.type) << std::endl;
                break;
            }
            break;

        default:
            std::cerr << "no access_token 非法消息类型: "
                      << static_cast<int>(msg.type)
                      << " 消息ID: " << msg.message_id << std::endl;
            send_error("非法消息类型", msg.message_id);
            break;
        }
    }

    // 需要 access_token 的消息类型
    else {
        // 验证 access_token 的合法性
        const std::string &jwt_token = *msg.jwt_token;
        if (SessionManager::get_instance().validate_jwt(user_id_, jwt_token,
                                                        true)) {
            switch (msg.type) {
            // 数据消息分支
            case MsgType::Data:
                switch (msg.content.type) { // 内容子类型判断
                case ContentType::Text:
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
                              << static_cast<int>(msg.content.type)
                              << std::endl;
                }
                break;

            // 控制消息分支
            case MsgType::Control:
                switch (msg.content.type) {
                case ContentType::Logout:
                    break;
                case ContentType::FriendRequest:
                    recv_friend_request_msg(msg);
                    break;
                case ContentType::ApproveFriendRequest:
                    recv_approve_friend_request_msg(msg);
                    break;
                case ContentType::DeleteFriend:
                    break;
                default:
                    std::cerr << "未知的控制消息类型: "
                              << static_cast<int>(msg.content.type)
                              << std::endl;
                }
                break;

            // 系统消息分支
            case MsgType::System:
                switch (msg.content.type) {
                default:
                    std::cerr << "未知的控制消息类型: "
                              << static_cast<int>(msg.content.type)
                              << std::endl;
                }
                break;

            default:
                std::cerr << "非法消息类型: " << static_cast<int>(msg.type)
                          << " 消息ID: " << msg.message_id << std::endl;
                send_error("非法消息类型", msg.message_id);
            }
        } else {
            // 将错误信息记录进日志
            Logger::instance().bug_log("jwt access token 不合法" +
                                       msg.message_id);
        }
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
    response_msg.message_id = generate_uuid();
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
        send_error("登录信息不完整", msg.message_id);
        return EXIT_FAILURE;
    }

    // 生成密码哈希
    std::string password_hash = do_hash(msg.content.data["password"]);

    auto conn = mysql_pool_.get_connection();
    std::string query = "SELECT id, nickname FROM users WHERE email = ? AND "
                        "password_hash = ? LIMIT 1";

    MYSQL_STMT *stmt = mysql_stmt_init(conn.get());
    if (!stmt)
        return EXIT_FAILURE;
    auto stmt_guard = std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)>(
        stmt, mysql_stmt_close);

    // 预处理语句
    if (mysql_stmt_prepare(stmt, query.c_str(), query.size())) {
        Logger::instance().log(Logger::ERROR,
                               "Prepare error: " +
                                   std::string(mysql_stmt_error(stmt)));
        return EXIT_FAILURE;
    }

    // 绑定参数
    std::string email = msg.content.data["email"];
    MYSQL_BIND params[2] = {};
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = email.data();
    params[0].buffer_length = email.size();

    params[1].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer = password_hash.data();
    params[1].buffer_length = password_hash.size();

    if (mysql_stmt_bind_param(stmt, params)) {
        Logger::instance().log(Logger::ERROR,
                               "Bind param error: " +
                                   std::string(mysql_stmt_error(stmt)));
        return EXIT_FAILURE;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        Logger::instance().log(Logger::ERROR,
                               "Execute error: " +
                                   std::string(mysql_stmt_error(stmt)));
        return EXIT_FAILURE;
    }

    // 绑定结果集
    char nickname_buf[51] = {0};
    unsigned long nickname_len = 0;

    MYSQL_BIND results[2] = {};
    // 绑定用户ID字段
    results[0].buffer_type = MYSQL_TYPE_LONGLONG; // 匹配BIGINT类型
    results[0].buffer = &user_id;                 // 存储到long long变量
    results[0].is_unsigned = true;                // 如果id是无符号类型

    // 绑定昵称字段
    results[1].buffer_type = MYSQL_TYPE_STRING;
    results[1].buffer = nickname_buf;
    results[1].buffer_length = sizeof(nickname_buf) - 1;
    results[1].length = &nickname_len;

    if (mysql_stmt_bind_result(stmt, results)) {
        Logger::instance().log(Logger::ERROR,
                               "Bind result error: " +
                                   std::string(mysql_stmt_error(stmt)));
        return EXIT_FAILURE;
    }

    // 存储结果集
    if (mysql_stmt_store_result(stmt)) {
        Logger::instance().log(Logger::ERROR,
                               "Store result error: " +
                                   std::string(mysql_stmt_error(stmt)));
        return EXIT_FAILURE;
    }

    // 检查结果是否存在
    if (mysql_stmt_num_rows(stmt) == 0) {
        Logger::instance().log(Logger::INFO, "用户不存在: " + email);
        send_error("邮箱或密码错误", msg.message_id);
        return EXIT_FAILURE;
    }

    // 提取数据
    if (mysql_stmt_fetch(stmt) != 0) {
        Logger::instance().log(Logger::ERROR,
                               "Fetch error: " +
                                   std::string(mysql_stmt_error(stmt)));
        return EXIT_FAILURE;
    }

    // 转换为字符串（核心步骤）
    user_id_ = std::to_string(user_id);
    nickname_buf[nickname_len] = '\0';
    std::string nickname(nickname_buf);

    // 验证转换结果
    if (user_id_.empty()) {
        Logger::instance().log(Logger::ERROR, "用户ID转换失败");
        return EXIT_FAILURE;
    }

    Logger::instance().log(Logger::INFO, "登录成功: user_id=" + user_id_);

    // 会话管理
    SessionManager::get_instance().add(user_id_, shared_from_this());

    // 发送响应
    Message response_msg{};
    response_msg.message_id = generate_uuid();      // 生成唯一消息ID
    response_msg.type = MsgType::Control;           // 主类型为控制消息
    response_msg.content.type = ContentType::Login; // 子类型为登录

    // 设置发送者信息
    response_msg.sender = UserInfo{
        .user_id = user_id_,  // 从会话获取用户ID
        .username = nickname, // 使用传入的昵称
        .avatar = ""          // 默认空头像
    };
    // 设置时间戳
    response_msg.timestamp = generate_timestamp();
    // 生成JWT令牌对
    std::pair<std::string, std::string> token;
    token = SessionManager::get_instance().generate_jwt(user_id_);
    // 访问令牌
    response_msg.jwt_token = token.second;
    // 刷新令牌
    response_msg.content.data["refresh_token"] = token.first;
    // 设置元数据
    response_msg.metadata = {
        .status = "success", .reply_to = msg.message_id, .is_encrypted = false};

    send(response_msg);

    return EXIT_SUCCESS;
}

int Session::recv_fresh_access_token_msg(const Message &msg) {
    // 从消息中提取用户ID
    std::string user_id = msg.sender.user_id;

    // 从消息内容中提取刷新令牌
    std::string refresh_token = msg.content.data;

    // 获取JWT管理器的单例实例
    auto &session_manager = SessionManager::get_instance();

    // 生成新的访问令牌
    std::string access_token =
        session_manager.generate_access_jwt(user_id, refresh_token);

    // 构造响应消息
    Message response_msg;
    response_msg.message_id = generate_uuid(); // 生成唯一消息ID
    response_msg.type = MsgType::Control;      // 设置为控制消息类型
    response_msg.content.type = ContentType::FreshToken;
    response_msg.sender = {"server", "", ""};      // 发送方标记为服务器
    response_msg.timestamp = generate_timestamp(); // 当前时间戳

    // 设置元数据
    response_msg.metadata = {.status =
                                 access_token.empty() ? "failure" : "success",
                             .reply_to = msg.message_id,
                             .is_encrypted = false};

    // 填充消息内容
    if (!access_token.empty()) {
        // 成功时将新访问令牌放入content
        response_msg.content.data = access_token;
    } else {
        // 失败时提供错误信息（可扩展为JSON结构）
        response_msg.content.data = R"({"error":"invalid refresh token"})";
        Logger::instance().log(Logger::ERROR, "Refresh failed for user: " +
                                                  user_id); // 记录错误日志
    }

    // 发送响应消息给客户端
    send(response_msg);

    // 返回操作结果
    return 0;
}

int Session::recv_friend_request_msg(const Message &msg) {
    // 检查必要参数
    if (!msg.content.data.contains("friend_id") &&
        !msg.content.data.contains("friend_email")) {
        send_error("好友信息不完整", msg.message_id);
        return EXIT_FAILURE;
    }

    // 解析好友标识
    long long int friend_id;
    std::string friend_identifier;
    bool is_email = false;
    if (msg.content.data.contains("friend_email")) {
        friend_identifier = msg.content.data["friend_email"];
        is_email = true;
    } else {
        friend_identifier = msg.content.data["friend_id"];
    }

    // 获取好友id
    if (is_email) {
        friend_id = get_id_by_email(friend_identifier);
    } else {
        friend_id = std::stoll(friend_identifier);
    }

    // 禁止添加自己为好友
    if (user_id_ == std::to_string(friend_id)) {
        send_error("不能添加自己为好友", msg.message_id);
        return EXIT_FAILURE;
    }

    // 检查用户是否存在
    auto conn = mysql_pool_.get_connection();
    std::string user_check = "SELECT id FROM users WHERE id = ? LIMIT 1";
    MYSQL_STMT *check_stmt = mysql_stmt_init(conn.get());
    if (!check_stmt) {
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }
    auto check_guard = std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)>(
        check_stmt, mysql_stmt_close);

    if (mysql_stmt_prepare(check_stmt, user_check.c_str(), user_check.size())) {
        Logger::instance().log(Logger::ERROR,
                               "Prepare error: " +
                                   std::string(mysql_stmt_error(check_stmt)));
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }

    MYSQL_BIND check_param = {};
    check_param.buffer_type = MYSQL_TYPE_LONGLONG;
    check_param.buffer = &friend_id;

    if (mysql_stmt_bind_param(check_stmt, &check_param)) {
        Logger::instance().log(Logger::ERROR,
                               "Bind error: " +
                                   std::string(mysql_stmt_error(check_stmt)));
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }

    if (mysql_stmt_execute(check_stmt)) {
        Logger::instance().log(Logger::ERROR,
                               "Execute error: " +
                                   std::string(mysql_stmt_error(check_stmt)));
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }

    if (mysql_stmt_store_result(check_stmt)) {
        Logger::instance().log(Logger::ERROR,
                               "Store error: " +
                                   std::string(mysql_stmt_error(check_stmt)));
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }

    if (mysql_stmt_fetch(check_stmt) != 0) {
        send_error("好友不存在", msg.message_id);
        return EXIT_FAILURE;
    }

    // 尝试插入好友关系
    std::string insert_query =
        "INSERT INTO friends (user_id, friend_id, status) VALUES (?, ?, 0)";
    MYSQL_STMT *insert_stmt = mysql_stmt_init(conn.get());
    if (!insert_stmt) {
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }
    auto insert_guard =
        std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)>(
            insert_stmt, mysql_stmt_close);

    if (mysql_stmt_prepare(insert_stmt, insert_query.c_str(),
                           insert_query.size())) {
        Logger::instance().log(Logger::ERROR,
                               "Prepare error: " +
                                   std::string(mysql_stmt_error(insert_stmt)));
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }

    MYSQL_BIND insert_params[2] = {};
    insert_params[0].buffer_type = MYSQL_TYPE_LONGLONG;
    insert_params[0].buffer = &user_id;
    insert_params[1].buffer_type = MYSQL_TYPE_LONGLONG;
    insert_params[1].buffer = &friend_id;

    if (mysql_stmt_bind_param(insert_stmt, insert_params)) {
        Logger::instance().log(Logger::ERROR,
                               "Bind error: " +
                                   std::string(mysql_stmt_error(insert_stmt)));
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }

    if (mysql_stmt_execute(insert_stmt)) {
        unsigned int error_code = mysql_stmt_errno(insert_stmt);
        if (error_code == 1062) { // ER_DUP_ENTRY
            send_error("已发送好友请求或已是好友", msg.message_id);
        } else {
            Logger::instance().log(
                Logger::ERROR,
                "Execute error: " + std::string(mysql_stmt_error(insert_stmt)));
            send_error("数据库错误", msg.message_id);
        }
        return EXIT_FAILURE;
    }

    // 发送成功响应
    Message response_msg;
    response_msg.message_id = generate_uuid();
    response_msg.type = MsgType::Control;
    response_msg.content.type = ContentType::FriendRequest;
    response_msg.sender = {user_id_, "", ""};
    response_msg.timestamp = generate_timestamp();
    response_msg.metadata = {
        .status = "success", .reply_to = msg.message_id, .is_encrypted = false};
    send(response_msg);

    return EXIT_SUCCESS;
}

int Session::recv_approve_friend_request_msg(const Message &msg) {
    // 检查必要参数：必须包含 friend_id/friend_email
    if (!msg.content.data.contains("friend_id") &&
        !msg.content.data.contains("friend_email")) {
        send_error("缺少 friend_id/friend_email", msg.message_id);
        return EXIT_FAILURE;
    }

    // 获取数据库连接
    auto conn = mysql_pool_.get_connection();
    if (!conn) {
        send_error("数据库连接失败", msg.message_id);
        return EXIT_FAILURE;
    }

    // 开启事务
    mysql_autocommit(conn.get(), false);

    // 根据 user_id 查询待处理的好友请求
    std::string query;
    MYSQL_STMT *stmt = nullptr;

    if (user_id) {
        query =
            "SELECT user_id FROM friends "
            "WHERE request_id = ? AND to_user_id = ? AND status = 'pending' "
            "FOR UPDATE"; // 加锁防止并发修改
    } else {
        query =
            "SELECT request_id FROM friend_requests "
            "WHERE from_user_id = ? AND to_user_id = ? AND status = 'pending' "
            "FOR UPDATE";
    }

    stmt = mysql_stmt_init(conn.get());
    if (!stmt) {
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }
    auto stmt_guard = std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)>(
        stmt, mysql_stmt_close);

    if (mysql_stmt_prepare(stmt, query.c_str(), query.size())) {
        Logger::instance().bug_log("Prepare error: " +
                                   std::string(mysql_stmt_error(stmt)));
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }

    // 绑定参数
    MYSQL_BIND params[2] = {};
    long long int request_id_or_from_user_id = 0;

    if (user_id) {
        // 使用 request_id 查询
        try {
            request_id_or_from_user_id =
                std::stoll(msg.content.data["request_id"]);
        } catch (...) {
            send_error("请求ID格式错误", msg.message_id);
            return EXIT_FAILURE;
        }
        params[0].buffer_type = MYSQL_TYPE_LONGLONG;
        params[0].buffer = &request_id_or_from_user_id;
    } else {
        // 使用 friend_id/friend_email 查询申请者ID
        std::string friend_identifier;
        if (msg.content.data.contains("friend_email")) {
            // 根据邮箱查询用户ID（代码类似recv_friend_request_msg）
            std::string email = msg.content.data["friend_email"];
            long long int from_user_id = 0;
            if (!get_id_by_email(email)) {
                send_error("申请者不存在", msg.message_id);
                return EXIT_FAILURE;
            }
            request_id_or_from_user_id = from_user_id;
        } else {
            try {
                request_id_or_from_user_id =
                    std::stoll(msg.content.data["friend_id"]);
            } catch (...) {
                send_error("好友ID格式错误", msg.message_id);
                return EXIT_FAILURE;
            }
        }
        params[0].buffer_type = MYSQL_TYPE_LONGLONG;
        params[0].buffer = &request_id_or_from_user_id;
    }

    // 第二个参数是当前用户的ID（接收者）
    params[1].buffer_type = MYSQL_TYPE_LONGLONG;
    params[1].buffer = &user_id_;

    if (mysql_stmt_bind_param(stmt, params)) {
        Logger::instance().bug_log("Bind param error: " +
                                   std::string(mysql_stmt_error(stmt)));
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }

    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        Logger::instance().bug_log("Execute error: " +
                                   std::string(mysql_stmt_error(stmt)));
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }

    // 获取结果
    MYSQL_BIND result = {};
    long long int from_user_id = 0; // 当使用request_id时，需要获取from_user_id
    if (use_request_id) {
        result.buffer_type = MYSQL_TYPE_LONGLONG;
        result.buffer = &from_user_id;
    } else {
        result.buffer_type = MYSQL_TYPE_LONGLONG;
        result.buffer = &request_id_or_from_user_id; // 此处存储实际request_id
    }

    if (mysql_stmt_bind_result(stmt, &result)) {
        Logger::instance().bug_log("Bind result error: " +
                                   std::string(mysql_stmt_error(stmt)));
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }

    if (mysql_stmt_store_result(stmt)) {
        Logger::instance().bug_log("Store result error: " +
                                   std::string(mysql_stmt_error(stmt)));
        send_error("数据库错误", msg.message_id);
        return EXIT_FAILURE;
    }

    // 检查是否存在待处理的请求
    if (mysql_stmt_fetch(stmt) != 0) {
        send_error("未找到待处理的好友请求", msg.message_id);
        mysql_rollback(conn.get());
        return EXIT_FAILURE;
    }

    // 如果未使用request_id，此时request_id_or_from_user_id是实际request_id
    const long long int actual_request_id = use_request_id
                                                ? request_id_or_from_user_id
                                                : request_id_or_from_user_id;
    const long long int actual_from_user_id =
        use_request_id ? from_user_id : request_id_or_from_user_id;

    // 更新请求状态为已批准
    std::string update_query =
        "UPDATE friend_requests SET status = 'approved', processed_at = NOW() "
        "WHERE request_id = ?";
    MYSQL_STMT *update_stmt = mysql_stmt_init(conn.get());
    if (!update_stmt) {
        send_error("数据库错误", msg.message_id);
        mysql_rollback(conn.get());
        return EXIT_FAILURE;
    }
    auto update_guard =
        std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)>(
            update_stmt, mysql_stmt_close);

    if (mysql_stmt_prepare(update_stmt, update_query.c_str(),
                           update_query.size())) {
        Logger::instance().bug_log("Prepare update error: " +
                                   std::string(mysql_stmt_error(update_stmt)));
        send_error("数据库错误", msg.message_id);
        mysql_rollback(conn.get());
        return EXIT_FAILURE;
    }

    MYSQL_BIND update_param = {};
    update_param.buffer_type = MYSQL_TYPE_LONGLONG;
    update_param.buffer = &actual_request_id;
    if (mysql_stmt_bind_param(update_stmt, &update_param)) {
        Logger::instance().bug_log("Bind update error: " +
                                   std::string(mysql_stmt_error(update_stmt)));
        send_error("数据库错误", msg.message_id);
        mysql_rollback(conn.get());
        return EXIT_FAILURE;
    }

    if (mysql_stmt_execute(update_stmt)) {
        Logger::instance().bug_log("Execute update error: " +
                                   std::string(mysql_stmt_error(update_stmt)));
        send_error("数据库错误", msg.message_id);
        mysql_rollback(conn.get());
        return EXIT_FAILURE;
    }

    // 插入双向好友关系到friends表
    std::string insert_query = "INSERT INTO friends (user_id, friend_id) "
                               "VALUES (?, ?), (?, ?)"; // 双向关系
    MYSQL_STMT *insert_stmt = mysql_stmt_init(conn.get());
    if (!insert_stmt) {
        send_error("数据库错误", msg.message_id);
        mysql_rollback(conn.get());
        return EXIT_FAILURE;
    }
    auto insert_guard =
        std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)>(
            insert_stmt, mysql_stmt_close);

    if (mysql_stmt_prepare(insert_stmt, insert_query.c_str(),
                           insert_query.size())) {
        Logger::instance().bug_log("Prepare insert error: " +
                                   std::string(mysql_stmt_error(insert_stmt)));
        send_error("数据库错误", msg.message_id);
        mysql_rollback(conn.get());
        return EXIT_FAILURE;
    }

    // 绑定参数：user_id_, actual_from_user_id 和 actual_from_user_id, user_id_
    long long int insert_params[4] = {user_id_, actual_from_user_id,
                                      actual_from_user_id, user_id_};

    MYSQL_BIND insert_binds[4] = {};
    for (int i = 0; i < 4; i++) {
        insert_binds[i].buffer_type = MYSQL_TYPE_LONGLONG;
        insert_binds[i].buffer = &insert_params[i];
    }

    if (mysql_stmt_bind_param(insert_stmt, insert_binds)) {
        Logger::instance().bug_log("Bind insert error: " +
                                   std::string(mysql_stmt_error(insert_stmt)));
        send_error("数据库错误", msg.message_id);
        mysql_rollback(conn.get());
        return EXIT_FAILURE;
    }

    if (mysql_stmt_execute(insert_stmt)) {
        // 处理唯一约束错误（避免重复添加好友）
        unsigned int err = mysql_stmt_errno(insert_stmt);
        if (err == 1062) { // ER_DUP_ENTRY
            Logger::instance().log(Logger::INFO, "好友关系已存在，忽略插入");
        } else {
            Logger::instance().bug_log(
                "Execute insert error: " +
                std::string(mysql_stmt_error(insert_stmt)));
            send_error("数据库错误", msg.message_id);
            mysql_rollback(conn.get());
            return EXIT_FAILURE;
        }
    }

    // 提交事务
    if (mysql_commit(conn.get())) {
        Logger::instance().bug_log("Commit error: " +
                                   std::string(mysql_error(conn.get())));
        send_error("数据库错误", msg.message_id);
        mysql_rollback(conn.get());
        return EXIT_FAILURE;
    }

    // 发送成功响应
    // send_success("好友请求已批准", msg.message_id);
    return EXIT_SUCCESS;
}

void Session::send(const Message &msg) {
    // 将消息转换为json格式
    auto msg_json = std::make_shared<std::string>(to_json(msg).dump());
    ws_.async_write(
        asio::buffer(*msg_json),
        [self = shared_from_this(), msg_json](beast::error_code ec, size_t) {
            if (ec) {
                self->close();
            } else {
                // 记录成功发送日志
                Logger::instance().network_log(
                    Logger::OUT,
                    self->socket().remote_endpoint().address().to_string(),
                    *msg_json, "");
            }
        });
}

void Session::send_error(const std::string e, const std::string msg_id) {
    std::cout << msg_id << ": " << e << std::endl;
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

long long int Session::get_id_by_email(const std::string &email) {
    auto conn = mysql_pool_.get_connection();
    std::string query = "SELECT id FROM users WHERE email = ? LIMIT 1";
    MYSQL_STMT *stmt = mysql_stmt_init(conn.get());
    if (!stmt) {
        send_error("数据库错误", "");
        return EXIT_FAILURE;
    }
    auto stmt_guard = std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)>(
        stmt, mysql_stmt_close);

    if (mysql_stmt_prepare(stmt, query.c_str(), query.size())) {
        Logger::instance().log(Logger::ERROR,
                               "Prepare error: " +
                                   std::string(mysql_stmt_error(stmt)));
        send_error("数据库错误", "");
        return EXIT_FAILURE;
    }

    MYSQL_BIND param = {};
    param.buffer_type = MYSQL_TYPE_STRING;
    param.buffer = const_cast<char *>(email.data());
    param.buffer_length = email.size();

    if (mysql_stmt_bind_param(stmt, &param)) {
        Logger::instance().log(Logger::ERROR,
                               "Bind error: " +
                                   std::string(mysql_stmt_error(stmt)));
        send_error("数据库错误", "");
        return EXIT_FAILURE;
    }

    if (mysql_stmt_execute(stmt)) {
        Logger::instance().log(Logger::ERROR,
                               "Execute error: " +
                                   std::string(mysql_stmt_error(stmt)));
        send_error("数据库错误", "");
        return EXIT_FAILURE;
    }

    MYSQL_BIND result = {};
    long long int fetched_id = 0;
    result.buffer_type = MYSQL_TYPE_LONGLONG;
    result.buffer = &fetched_id;

    if (mysql_stmt_bind_result(stmt, &result)) {
        Logger::instance().log(Logger::ERROR,
                               "Bind result error: " +
                                   std::string(mysql_stmt_error(stmt)));
        send_error("数据库错误", "");
        return EXIT_FAILURE;
    }

    if (mysql_stmt_store_result(stmt)) {
        Logger::instance().log(Logger::ERROR,
                               "Store result error: " +
                                   std::string(mysql_stmt_error(stmt)));
        send_error("数据库错误", "");
        return EXIT_FAILURE;
    }

    if (mysql_stmt_fetch(stmt) != 0) {
        send_error("用户不存在", "");
        return EXIT_FAILURE;
    }
    return fetched_id;
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
