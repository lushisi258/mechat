#include "../include/network_logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace IM {

// 单例实现
NetworkLogger &NetworkLogger::instance() {
    static NetworkLogger logger;
    return logger;
}

// 构造函数
NetworkLogger::NetworkLogger() {
    std::thread([this] {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            async_write();
        }
    }).detach();
}

// 静态初始化方法
void NetworkLogger::init(Level console_level, Level file_level,
                         const std::string &log_path) {
    instance().initImpl(console_level, file_level, log_path);
}

// 实际初始化逻辑
void NetworkLogger::initImpl(Level console_level, Level file_level,
                             const std::string &log_path) {
    console_level_ = console_level;
    file_level_ = file_level;
    if (!log_path.empty()) {
        log_file_.open(log_path, std::ios::app);
    }
}

// 静态过滤器添加方法
void NetworkLogger::add_filter(Filter filter) {
    instance().filters_.push_back(filter);
}

void NetworkLogger::log(Level level, const std::string &direction,
                        const net::ip::tcp::endpoint &endpoint,
                        const std::string &raw_data,
                        const std::string &parsed_data) {
    // 应用过滤器
    for (auto &filter : filters_) {
        if (!filter(raw_data))
            return;
    }

    // 生成日志条目
    LogEntry entry;
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;
    auto timer = std::chrono::system_clock::to_time_t(now);

    std::ostringstream timestamp;
    timestamp << std::put_time(std::localtime(&timer), "%F %T.")
              << std::setfill('0') << std::setw(3) << ms.count();

    entry.timestamp = timestamp.str();
    entry.level = level;
    entry.direction = direction;
    entry.endpoint =
        endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
    entry.raw_hex = generate_hex_dump(raw_data);
    entry.parsed_json = parsed_data;

    // 非阻塞写入缓冲区
    std::lock_guard lock(buffer_mutex_);
    front_buffer_.push(std::move(entry));
}

std::string NetworkLogger::generate_hex_dump(const std::string &data) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : data) {
        ss << std::setw(2) << static_cast<int>(c) << ' ';
    }
    return ss.str();
}

void NetworkLogger::async_write() {
    if (writing_.exchange(true))
        return;

    // 交换缓冲区
    std::queue<LogEntry> write_buffer;
    {
        std::lock_guard lock(buffer_mutex_);
        front_buffer_.swap(back_buffer_);
        write_buffer.swap(back_buffer_);
    }

    // 写入日志
    while (!write_buffer.empty()) {
        const auto &entry = write_buffer.front();

        // 控制台输出
        if (entry.level >= console_level_) {
            std::clog << format_entry(entry) << std::endl;
        }

        // 文件输出
        if (log_file_.is_open() && entry.level >= file_level_) {
            log_file_ << format_entry(entry) << std::endl;
        }

        write_buffer.pop();
    }

    writing_.store(false);
}

std::string NetworkLogger::format_entry(const LogEntry &entry) {
    std::ostringstream ss;
    ss << "[" << entry.timestamp << "]"
       << "[" << entry.endpoint << "]"
       << "[" << entry.direction << "]"
       << "[RAW] " << entry.raw_hex << "\n"
       << "[PARSE] " << entry.parsed_json;
    return ss.str();
}

NetworkLogger::~NetworkLogger() {}

} // namespace IM