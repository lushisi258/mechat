// database_pool.hpp
#pragma once
#ifndef DATABASE_POOL_HPP
#define DATABASE_POOL_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <variant>

// MariaDB 头文件
#include <mariadb/errmsg.h>
#include <mariadb/mysql.h>
#include <mariadb/mysqld_error.h>

// Redis 头文件
#include <hiredis/hiredis.h>

// MongoDB 头文件
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/client.hpp>
#include <mongocxx/pool.hpp>
#include <mongocxx/uri.hpp>

// MySQL 连接池（MariaDB 实现）
class MySQLConnectionPool {
  public:
    explicit MySQLConnectionPool(const std::string &host,
                                 const std::string &user,
                                 const std::string &password,
                                 const std::string &database, unsigned int port,
                                 size_t max_pool_size = 10);

    MySQLConnectionPool(const MySQLConnectionPool &) = delete;
    MySQLConnectionPool &operator=(const MySQLConnectionPool &) = delete;

    std::unique_ptr<MYSQL, std::function<void(MYSQL *)>> get_connection();
    void release_connection(MYSQL *conn);

    ~MySQLConnectionPool();

  private:
    MYSQL *create_raw_connection();

    std::mutex pool_mutex_;
    std::queue<MYSQL *> pool_;
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    unsigned int port_;
    size_t max_pool_size_;
    size_t active_connections_ = 0;
};

// Redis 连接池
class RedisConnectionPool {
  public:
    explicit RedisConnectionPool(const std::string &host, int port,
                                 size_t max_pool_size = 10);

    RedisConnectionPool(const RedisConnectionPool &) = delete;
    RedisConnectionPool &operator=(const RedisConnectionPool &) = delete;

    std::unique_ptr<redisContext, std::function<void(redisContext *)>>
    get_connection();
    void release_connection(
        std::unique_ptr<redisContext, void (*)(redisContext *)> conn);

    ~RedisConnectionPool();

  private:
    std::mutex pool_mutex_;
    std::queue<std::unique_ptr<redisContext, void (*)(redisContext *)>> pool_;
    std::string host_;
    int port_;
    size_t max_pool_size_;
    size_t active_connections_ = 0;
};

// MongoDB 连接池
class MongoDBConnectionPool {
  public:
     explicit MongoDBConnectionPool(const std::string &uri,
                                    size_t min_pool_size = 5,
                                    size_t max_pool_size = 20);
 
     MongoDBConnectionPool(const MongoDBConnectionPool &) = delete;
     MongoDBConnectionPool &operator=(const MongoDBConnectionPool &) = delete;
 
     mongocxx::client get_connection();
 
  private:
     static mongocxx::instance instance_;
     mongocxx::pool connection_pool_;
 };    

// 数据库操作工具类
class DatabaseUtil {
  public:
    using ParamType =
        std::variant<uint64_t, std::string, double, std::nullptr_t>;
    using ResultHandler =
        std::function<void(const MYSQL_ROW row, unsigned long *lengths)>;

    explicit DatabaseUtil(MySQLConnectionPool &pool);

    void execute_query(const std::string &query,
                       const std::vector<ParamType> &params,
                       ResultHandler handler = nullptr);

    bool exists(const std::string &query, const std::vector<ParamType> &params);
    uint64_t insert(const std::string &query,
                    const std::vector<ParamType> &params);

  private:
    std::vector<MYSQL_BIND>
    bind_parameters(const std::vector<ParamType> &params,
                    std::vector<std::unique_ptr<char[]>> &str_buffers);

    void process_results(MYSQL_STMT *stmt, ResultHandler handler);

    MySQLConnectionPool &pool_;
};

#endif // DATABASE_POOL_HPP