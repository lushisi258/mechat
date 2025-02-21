// database_pool.hpp
#pragma once
#ifndef DATABASE_POOL_HPP
#define DATABASE_POOL_HPP

#include <memory>
#include <mutex>
#include <queue>
#include <string>

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

    std::unique_ptr<MYSQL, std::function<void(MYSQL *)>> getConnection();
    void releaseConnection(MYSQL *conn);

    ~MySQLConnectionPool();

  private:
    std::mutex pool_mutex_;
    std::queue<MYSQL *> pool_;
    std::string host_, user_, password_, database_;
    unsigned int port_;
    size_t max_pool_size_;
    size_t active_connections_ = 0;

    MYSQL *createRawConnection();
};

// Redis 连接池
class RedisConnectionPool {
  public:
    explicit RedisConnectionPool(const std::string &host, int port,
                                 size_t max_pool_size = 10);

    RedisConnectionPool(const RedisConnectionPool &) = delete;
    RedisConnectionPool &operator=(const RedisConnectionPool &) = delete;

    std::unique_ptr<redisContext, void (*)(redisContext *)> getConnection();
    void releaseConnection(
        std::unique_ptr<redisContext, void (*)(redisContext *)> conn);

    ~RedisConnectionPool();

  private:
    std::mutex pool_mutex_;
    std::queue<std::unique_ptr<redisContext, void (*)(redisContext *)>> pool_;
    const std::string host_;
    const int port_;
    const size_t max_pool_size_;
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

    mongocxx::client getConnection();

  private:
    static mongocxx::instance instance_;
    mongocxx::pool connection_pool_;
};

#endif // DATABASE_POOL_HPP