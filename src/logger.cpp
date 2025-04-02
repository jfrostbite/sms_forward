#include "logger.hpp"
#include "config.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cxxabi.h>

// 全局变量指向Logger实例
static Logger* g_logger = nullptr;

// 信号处理函数
void signalHandler(int sig) {
    if (g_logger) {
        g_logger->logCrash(sig);
    }

    // 重置信号处理并继续崩溃
    signal(sig, SIG_DFL);
    raise(sig);
}

Logger& Logger::getInstance() {
    static Logger instance;
    g_logger = &instance; // 设置全局指针
    return instance;
}

bool Logger::init(const std::string& log_path) {
    log_file.open(log_path, std::ios::app);
    bool success = log_file.is_open();

    if (success) {
        // 初始化信号处理
        initSignalHandlers();
    }

    return success;
}

void Logger::initSignalHandlers() {
    // 注册常见的崩溃信号
    signal(SIGSEGV, signalHandler); // 段错误
    signal(SIGABRT, signalHandler); // abort() 调用
    signal(SIGFPE, signalHandler);  // 浮点异常
    signal(SIGILL, signalHandler);  // 非法指令
    signal(SIGBUS, signalHandler);  // 总线错误

    log("INFO", "Signal handlers initialized");
}

void Logger::logCrash(int sig) {
    std::string signame;
    switch (sig) {
        case SIGSEGV: signame = "SIGSEGV (Segmentation fault)"; break;
        case SIGABRT: signame = "SIGABRT (Abort)"; break;
        case SIGFPE:  signame = "SIGFPE (Floating point exception)"; break;
        case SIGILL:  signame = "SIGILL (Illegal instruction)"; break;
        case SIGBUS:  signame = "SIGBUS (Bus error)"; break;
        default:      signame = "Signal " + std::to_string(sig); break;
    }

    log("FATAL", "Program crashed with signal: " + signame);

    // 获取并记录调用栈
    std::string stacktrace = getStackTrace();
    log("FATAL", "Stack trace:\n" + stacktrace);

    // 确保日志写入磁盘
    log_file.flush();
}

std::string Logger::getStackTrace() {
    const int max_frames = 64;
    void* addrlist[max_frames];

    // 获取调用栈地址
    int addrlen = backtrace(addrlist, max_frames);
    if (addrlen == 0) {
        return "<empty stack trace, possibly corrupt>";
    }

    // 解析地址为符号
    char** symbollist = backtrace_symbols(addrlist, addrlen);
    if (symbollist == nullptr) {
        return "<failed to get backtrace symbols>";
    }

    std::stringstream ss;
    for (int i = 1; i < addrlen; i++) { // 跳过第一帧（当前函数）
        ss << "#" << (i-1) << ": " << symbollist[i] << "\n";

        // 尝试解析符号名（去除名称修饰）
        char* begin_name = nullptr;
        char* begin_offset = nullptr;
        char* end_offset = nullptr;

        // 查找符号名称的开始和结束
        for (char* p = symbollist[i]; *p; ++p) {
            if (*p == '(') {
                begin_name = p;
            } else if (*p == '+' && begin_name) {
                begin_offset = p;
            } else if (*p == ')' && begin_offset) {
                end_offset = p;
                break;
            }
        }

        if (begin_name && begin_offset && end_offset && begin_name < begin_offset) {
            *begin_name++ = '\0';
            *begin_offset++ = '\0';
            *end_offset = '\0';

            // 解析符号名
            int status;
            char* demangled = abi::__cxa_demangle(begin_name, nullptr, nullptr, &status);
            if (status == 0 && demangled) {
                ss << "    demangled: " << demangled << " + " << begin_offset << "\n";
                free(demangled);
            }
        }
    }

    free(symbollist);
    return ss.str();
}

void Logger::log(const std::string& level, const std::string& message) {
    if (!log_file.is_open()) return;

    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

    std::lock_guard<std::mutex> lock(mutex);
    log_file << "[" << ss.str() << "][" << level << "] " << message << std::endl;
    log_file.flush();
}

void Logger::info(const std::string& message) { log("INFO", message); }
void Logger::error(const std::string& message) { log("ERROR", message); }
void Logger::debug(const std::string& message) {
    // 只有在调试模式开启时才记录调试信息
    if (Config::getInstance().getDebugMode()) {
        log("DEBUG", message);
    }
}
void Logger::warning(const std::string& message) { log("WARNING", message); }
