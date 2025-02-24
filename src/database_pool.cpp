// database_pool.cpp
#include "database_pool.hpp"
#include <stdexcept>

// MySQLConnectionPool
MySQLConnectionPool::MySQLConnectionPool(const std::string &host,
                                         const std::string &user,
                                         const std::string &password,
                                         const std::string &database,
                                         unsigned int port,
                                         size_t max_pool_size)
    : host_(host), user_(user), password_(password), database_(database),
      port_(port), max_pool_size_(max_pool_size) {
    mysql_library_init(0, nullptr, nullptr);
}

MYSQL *MySQLConnectionPool::create_raw_connection() {
    MYSQL *conn = mysql_init(nullptr);
    if (!conn)
        throw std::runtime_error("mysql_init failed");

    my_bool reconnect = 1;
    mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

    if (!mysql_real_connect(conn, host_.c_str(), user_.c_str(),
                            password_.c_str(), database_.c_str(), port_,
                            nullptr, 0)) {
        std::string err = mysql_error(conn);
        mysql_close(conn);
        throw std::runtime_error("Connection failed: " + err);
    }

    mysql_set_character_set(conn, "utf8mb4");
    return conn;
}

std::unique_ptr<MYSQL, std::function<void(MYSQL *)>>

MySQLConnectionPool::get_connection() {
    std::unique_lock<std::mutex> lock(pool_mutex_);

    if (!pool_.empty()) {
        auto conn = pool_.front();
        pool_.pop();
        // 使用 lambda 捕获 this 并指定删除器
        return std::unique_ptr<MYSQL, std::function<void(MYSQL *)>>(
            conn, [this](MYSQL *c) { release_connection(c); });
    }

    if (active_connections_ >= max_pool_size_) {
        throw std::runtime_error("Connection pool exhausted");
    }

    MYSQL *raw_conn = create_raw_connection();
    active_connections_++;
    return std::unique_ptr<MYSQL, std::function<void(MYSQL *)>>(
        raw_conn, [this](MYSQL *c) { release_connection(c); });
}

void MySQLConnectionPool::release_connection(MYSQL *conn) {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    if (mysql_ping(conn) != 0) { // 检查连接有效性
        mysql_close(conn);
        active_connections_--;
    } else {
        pool_.push(conn);
    }
}

MySQLConnectionPool::~MySQLConnectionPool() {
    while (!pool_.empty()) {
        mysql_close(pool_.front());
        pool_.pop();
    }
    mysql_library_end();
}

// RedisConnectionPool
RedisConnectionPool::RedisConnectionPool(const std::string &host, int port,
                                         size_t max_pool_size)
    : host_(host), port_(port), max_pool_size_(max_pool_size) {}

std::unique_ptr<redisContext, void (*)(redisContext *)>
RedisConnectionPool::get_connection() {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    if (!pool_.empty()) {
        auto conn = std::move(pool_.front());
        pool_.pop();
        return conn;
    }

    if (active_connections_ >= max_pool_size_) {
        throw std::runtime_error("Redis connection pool exhausted");
    }

    redisContext *raw_conn = redisConnect(host_.c_str(), port_);
    if (!raw_conn || raw_conn->err) {
        if (raw_conn)
            redisFree(raw_conn);
        throw std::runtime_error("Redis connection failed");
    }

    active_connections_++;
    return std::unique_ptr<redisContext, void (*)(redisContext *)>(
        raw_conn, [](redisContext *ctx) { redisFree(ctx); });
}

void RedisConnectionPool::release_connection(
    std::unique_ptr<redisContext, void (*)(redisContext *)> conn) {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    // 发送PING保持连接活跃
    redisReply *reply = (redisReply *)redisCommand(conn.get(), "PING");
    if (reply && reply->type != REDIS_REPLY_ERROR) {
        freeReplyObject(reply);
        pool_.push(std::move(conn));
    } else {
        active_connections_--; // 无效连接直接销毁
    }
}

RedisConnectionPool::~RedisConnectionPool() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    while (!pool_.empty()) {
        pool_.pop(); // unique_ptr 自动释放
    }
}

// MongoDBConnectionPool
// 初始化静态成员
mongocxx::instance MongoDBConnectionPool::instance_{};

MongoDBConnectionPool::MongoDBConnectionPool(const std::string &uri,
                                             size_t min_pool_size,
                                             size_t max_pool_size)
    : connection_pool_(mongocxx::uri(uri), mongocxx::options::pool()) {}

mongocxx::client MongoDBConnectionPool::get_connection() {
    auto entry = connection_pool_.acquire();
    return std::move(*entry);
}