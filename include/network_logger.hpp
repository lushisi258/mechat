#pragma once
#include <atomic>
#include <boost/asio.hpp>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>

namespace IM {
namespace net = boost::asio;

class NetworkLogger {
  public:
    enum Level { DEBUG, INFO, WARNING, ERROR };
    using Filter = std::function<bool(const std::string &)>;

    // 单例访问
    static NetworkLogger &instance();

    // 初始化配置
    static void init(Level console_level = INFO, Level file_level = DEBUG,
                     const std::string &log_path = "../logs/network.log");

    // 日志记录接口
    void log(Level level, const std::string &direction,
             const net::ip::tcp::endpoint &endpoint,
             const std::string &raw_data, const std::string &parsed_data);

    // 过滤器管理
    static void add_filter(Filter filter);
    void clear_filters();

    // 生命周期管理
    void flush();
    void rotate_log(const std::string &new_path);

  private:
    NetworkLogger();
    ~NetworkLogger();

    // 实际初始化实现
    void initImpl(Level console_level, Level file_level,
                  const std::string &log_path);
    struct LogEntry {
        std::string timestamp;
        Level level;
        std::string endpoint;
        std::string direction;
        std::string raw_hex;
        std::string parsed_json;
    };

    // 双缓冲队列
    std::queue<LogEntry> front_buffer_;
    std::queue<LogEntry> back_buffer_;
    std::mutex buffer_mutex_;
    std::atomic<bool> writing_{false};

    // 配置项
    Level console_level_;
    Level file_level_;
    std::ofstream log_file_;
    std::vector<Filter> filters_;

    void async_write();
    std::string format_entry(const LogEntry &entry);
    std::string generate_hex_dump(const std::string &data);
};
} // namespace IM