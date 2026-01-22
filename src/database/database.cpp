// database_pool.cpp
#include "../include/database.hpp"
#include <cstring>
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
    if (!conn) {
        throw std::runtime_error("mysql_init failed");
    }

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
        active_connections_++;
        return {conn, [this](MYSQL *c) { release_connection(c); }};
    }

    if (active_connections_ >= max_pool_size_) {
        throw std::runtime_error("Connection pool exhausted");
    }

    MYSQL *raw_conn = create_raw_connection();
    active_connections_++;
    return {raw_conn, [this](MYSQL *c) { release_connection(c); }};
}

void MySQLConnectionPool::release_connection(MYSQL *conn) {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    bool valid = (mysql_ping(conn) == 0);
    if (valid && pool_.size() < max_pool_size_) {
        pool_.push(conn);
    } else {
        mysql_close(conn);
    }

    active_connections_--;
}

MySQLConnectionPool::~MySQLConnectionPool() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
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

std::unique_ptr<redisContext, std::function<void(redisContext *)>>
RedisConnectionPool::get_connection() {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    if (!pool_.empty()) {
        auto conn = std::move(pool_.front());
        pool_.pop();
        active_connections_++;
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
    return {raw_conn,
            [this](redisContext *ctx) { // 使用std::function允许捕获this
                release_connection(
                    std::unique_ptr<redisContext, void (*)(redisContext *)>(
                        ctx,
                        redisFree // 显式指定删除器
                        ));
            }};
}

void RedisConnectionPool::release_connection(
    std::unique_ptr<redisContext, void (*)(redisContext *)> conn) {
    std::lock_guard<std::mutex> lock(pool_mutex_);

    bool valid = false;
    if (conn && !conn->err) { // 增强有效性检查
        redisReply *reply =
            static_cast<redisReply *>(redisCommand(conn.get(), "PING"));
        if (reply) {
            valid = (reply->type != REDIS_REPLY_ERROR);
            freeReplyObject(reply);
        }
    }

    if (valid && pool_.size() < max_pool_size_) {
        pool_.push(std::move(conn));
    }
    // 如果无效，unique_ptr的删除器会自动调用redisFree

    active_connections_--;
}

RedisConnectionPool::~RedisConnectionPool() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    while (!pool_.empty()) {
        auto &conn = pool_.front();
        if (conn) { // 显式释放资源
            redisFree(conn.get());
        }
        pool_.pop();
    }
}

// MongoDBConnectionPool
mongocxx::instance MongoDBConnectionPool::instance_{};

MongoDBConnectionPool::MongoDBConnectionPool(const std::string &uri,
                                             size_t min_pool_size,
                                             size_t max_pool_size) {
    mongocxx::options::pool pool_options;
    try {
        connection_pool_ = mongocxx::pool(mongocxx::uri(uri), pool_options);
    } catch (const std::exception &e) {
        std::cerr << "Failed to create MongoDB connection pool: " << e.what()
                  << std::endl;
    }
}

mongocxx::client MongoDBConnectionPool::get_connection() {
    auto entry = connection_pool_.acquire();
    if (!entry) {
        std::cerr << "Failed to acquire a MongoDB connection from the pool."
                  << std::endl;
        throw std::runtime_error("Failed to acquire MongoDB connection");
    }
    return std::move(*entry);
}