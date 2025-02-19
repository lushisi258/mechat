#pragma once
#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <thread>

namespace IM {

class Logger {
  public:
    enum direction { IN, OUT };
    enum Level { DEBUG, INFO, WARNING, ERROR };
    using Filter = std::function<bool(const std::string &)>;

    // 单例访问
    static Logger &instance();

    // 初始化配置
    static void init(Level console_level = INFO, Level file_level = DEBUG,
                     const std::string &log_path = "../logs/asiowork.log");

    // 日志记录接口
    void log(Level level, const std::string &content);

    // bug日志记录
    void bug_log(const std::string &content);

    // 网络日志记录接口
    void network_log(const direction &direction_, const std::string &endpoint,
                     const std::string &raw_data,
                     const std::string &parsed_data);

    // 过滤器管理
    static void add_filter(Filter filter);
    void clear_filters();

    // 生命周期管理
    void flush();
    void rotate_log(const std::string &new_path);

  private:
    Logger();
    ~Logger();

    // 实际初始化实现
    void initImpl(Level console_level, Level file_level,
                  const std::string &log_path);

    struct LogEntry {
        std::string timestamp; // 时间
        Level level;           // 日志等级
        std::string content;   // 普通日志 & Bug 日志的内容

        // network_log
        std::optional<enum direction> direction; // "IN" 或 "OUT"
        std::optional<std::string> endpoint;     // 目标/来源地址
        std::optional<std::string> raw_hex;      // 原始数据的十六进制表示
        std::optional<std::string> parsed_json;  // 解析后的 JSON 数据
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

    // 获取时间戳
    std::string get_current_time();
    // 将日志条目写入文件
    void async_write();
    // 格式化日志条目
    std::string format_entry(const LogEntry &entry);
    // 将原始数据转化为16进制
    std::string generate_hex_dump(const std::string &data);
};
} // namespace IM