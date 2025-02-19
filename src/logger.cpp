#include "../include/logger.hpp"

namespace IM {

// 单例实现
Logger &Logger::instance() {
    static Logger logger;
    return logger;
}

// 构造函数
Logger::Logger() {
    std::thread([this] {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            async_write();
        }
    }).detach();
}

// 静态初始化方法
void Logger::init(Level console_level, Level file_level,
                  const std::string &log_path) {
    instance().initImpl(console_level, file_level, log_path);
}

// 实际初始化逻辑
void Logger::initImpl(Level console_level, Level file_level,
                      const std::string &log_path) {
    console_level_ = console_level;
    file_level_ = file_level;
    if (!log_path.empty()) {
        log_file_.open(log_path, std::ios::app);
    }
}

// 静态过滤器添加方法
void Logger::add_filter(Filter filter) {
    instance().filters_.push_back(filter);
}

// 基础日志记录接口
void Logger::log(Level level, const std::string &content) {
    LogEntry entry;
    entry.timestamp = get_current_time();
    entry.level = level;
    entry.content = content;

    // 非阻塞写入缓冲区
    std::lock_guard lock(buffer_mutex_);
    front_buffer_.push(std::move(entry));
}

// Bug日志记录
void Logger::bug_log(const std::string &content) {
    LogEntry entry;
    entry.timestamp = get_current_time();
    entry.level = Level::ERROR;
    entry.content = content;

    // 非阻塞写入缓冲区
    std::lock_guard lock(buffer_mutex_);
    front_buffer_.push(std::move(entry));
}

// 网络日志记录接口
void Logger::network_log(const direction &direction_, const std::string &ip_,
                         const std::string &raw_data,
                         const std::string &parsed_data) {
    LogEntry entry;
    entry.timestamp = get_current_time();
    entry.level = Level::INFO;
    entry.content = ip_;
    entry.direction = direction_;
    entry.raw_hex = generate_hex_dump(raw_data); // 原始数据转为十六进制
    entry.parsed_json = parsed_data;             // 解析后的数据

    // 非阻塞写入缓冲区
    std::lock_guard lock(buffer_mutex_);
    front_buffer_.push(std::move(entry));
}

std::string Logger::generate_hex_dump(const std::string &data) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned char c : data) {
        ss << std::setw(2) << static_cast<int>(c) << ' ';
    }
    return ss.str();
}

// 获取当前时间戳
std::string Logger::get_current_time() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()) %
              1000;
    auto timer = std::chrono::system_clock::to_time_t(now);

    std::ostringstream timestamp;
    timestamp << std::put_time(std::localtime(&timer), "%F %T.")
              << std::setfill('0') << std::setw(3) << ms.count();
    return timestamp.str();
}

void Logger::async_write() {
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

std::string Logger::format_entry(const LogEntry &entry) {
    std::ostringstream ss;
    ss << "[" << entry.timestamp << "]"
       << "[" << entry.level << "]"
       << "[" << entry.content << "]";

    if (entry.direction)
        ss << "[" << *entry.direction << "]";
    if (entry.endpoint)
        ss << "[" << *entry.endpoint << "]";
    if (entry.raw_hex)
        ss << "[RAW] " << *entry.raw_hex;
    if (entry.parsed_json)
        ss << "[PARSE] " << *entry.parsed_json;

    return ss.str();
}

Logger::~Logger() {}

} // namespace IM