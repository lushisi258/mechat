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

        auto msg = Message::from_json(beast::buffers_to_string(buffer_.data()));
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
    switch (msg.type) {
    case MsgType::Text:
        if (msg.receiver == "broadcast") {
            SessionManager::get_instance().broadcast(msg);
        } else if (user_id_) {
            SessionManager::get_instance().send_to_user(user_id_, msg);
        }
        break;
    case MsgType::Image:
        break;
    case MsgType::Heartbeat:
        heartbeat_timer_.cancel();
        start_heartbeat_timer();
        break;
    case MsgType::Login:
        // 实现认证逻辑
        recv_login_msg(msg);
        break;
    case MsgType::Register:
        // 处理注册消息
        recv_register_msg(msg);
        break;
    case MsgType::StatusNotify:
        break;
    default:
        // 错误的消息类型
        std::cout << "收到错误的消息类型" << std::endl;
    }
}

int Session::recv_register_msg(const Message &msg) {
    std::string password_hash;
    password_hash = do_hash(msg.meta);

    // 查询语句
    auto conn = mysql_pool_.get_connection();
    std::string query =
        "INSERT INTO users (email, password_hash) VALUES (?, ?)";

    MYSQL_STMT *stmt = mysql_stmt_init(conn.get());
    if (!stmt)
        return EXIT_FAILURE;

    auto stmt_guard = std::unique_ptr<MYSQL_STMT, decltype(&mysql_stmt_close)>(
        stmt, mysql_stmt_close);

    if (mysql_stmt_prepare(stmt, query.c_str(), query.size())) {
        std::cerr << "Prepare error: " << mysql_stmt_error(stmt) << std::endl;
        return EXIT_FAILURE;
    }

    // 参数绑定
    std::string email = msg.sender;
    MYSQL_BIND params[2] = {};
    params[0].buffer_type = MYSQL_TYPE_STRING;
    params[0].buffer = email.data();
    params[0].buffer_length = email.size();

    params[1].buffer_type = MYSQL_TYPE_STRING;
    params[1].buffer = password_hash.data();
    params[1].buffer_length = password_hash.size();

    if (mysql_stmt_bind_param(stmt, params)) {
        std::cerr << "Bind error: " << mysql_stmt_error(stmt) << std::endl;
        return EXIT_FAILURE;
    }

    // 执行插入
    if (mysql_stmt_execute(stmt)) {
        if (mysql_errno(conn.get()) == 1062) {
            std::cerr << "Email already exists: " << email << std::endl;
            return EXIT_FAILURE;
        }
        std::cerr << "Execute error: " << mysql_stmt_error(stmt) << std::endl;
        return EXIT_FAILURE;
    }

    // 获取插入的 ID
    user_id_ = mysql_insert_id(conn.get());
    if (user_id_ <= 0) {
        std::cerr << "Failed to get last insert ID" << std::endl;
        return EXIT_FAILURE;
    }

    // 会话管理
    SessionManager::get_instance().add(shared_from_this(), user_id_);

    // 发送响应
    Message response_msg{};
    std::pair<std::string, std::string> tokens;
    tokens = SessionManager::get_instance().generate_jwt(email);

    response_msg.type = MsgType::Login;
    response_msg.timestamp =
        std::chrono::system_clock::now().time_since_epoch().count();
    response_msg.meta = tokens.first;
    response_msg.token = tokens.second;

    send(response_msg);

    return EXIT_SUCCESS;
}

int Session::recv_login_msg(const Message &msg) {
    // 处理密码
    std::string password_hash = do_hash(msg.meta);

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
    std::string email = msg.sender;
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
    SessionManager::get_instance().add(shared_from_this(), user_id_);

    // 发送响应
    Message response_msg{};
    std::pair<std::string, std::string> tokens;
    // tokens.first 是 refresh token, tokens.second 是 access token
    tokens = SessionManager::get_instance().generate_jwt(email);

    response_msg.type = MsgType::Login;
    response_msg.sender = nickname;
    response_msg.timestamp =
        std::chrono::system_clock::now().time_since_epoch().count();
    response_msg.meta = tokens.first;
    response_msg.token = tokens.second;

    send(response_msg);

    return EXIT_SUCCESS;
}

int Session::recv_fresh_access_token_msg(const Message &msg) {
    std::string access_token;
    try {
        access_token = SessionManager::get_instance().generate_access_jwt(
            msg.sender, msg.meta);
    } catch (const std::exception &e) {
        std::string error_info =
            "Fresh access token failed: " + std::string(e.what());
        Logger::instance().log(Logger::ERROR, error_info);
        return EXIT_FAILURE;
    }

    Message response_msg{};

    response_msg.type = MsgType::UpdateJWT;
    response_msg.timestamp =
        std::chrono::system_clock::now().time_since_epoch().count();
    response_msg.token = access_token;

    send(response_msg);

    return EXIT_SUCCESS;
}

void Session::send(const Message &msg) {
    // 将消息转换为json格式
    std::string msg_json = msg.to_json();
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

} // namespace IM
