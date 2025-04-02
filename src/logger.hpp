/*
 * SMS Forward - A utility to forward SMS messages to WxPusher
 *
 * Copyright (c) 2025 J.Kev.Fen
 * All rights reserved.
 *
 * This software is proprietary and confidential.
 * Use is subject to license terms.
 *
 * You may NOT modify, adapt, or create derivative works based on this software.
 * You may NOT use this software for commercial purposes.
 * You may NOT distribute, sublicense, or make this software available to any third party.
 */

#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <csignal>
#include <execinfo.h>
#include <cstdlib>
#include <unistd.h>

// 信号处理函数声明
void signalHandler(int sig);

class Logger {
public:
    static Logger& getInstance();
    bool init(const std::string& log_path);
    void log(const std::string& level, const std::string& message);

    // 初始化信号处理
    void initSignalHandlers();

    // 记录崩溃信息
    void logCrash(int sig);

    // 获取调用栈信息
    std::string getStackTrace();

    void info(const std::string& message);
    void error(const std::string& message);
    void debug(const std::string& message);
    void warning(const std::string& message);

private:
    Logger() = default;
    std::ofstream log_file;
    std::mutex mutex;
};

#define LOG_INFO(msg) Logger::getInstance().info(msg)
#define LOG_ERROR(msg) Logger::getInstance().error(msg)
#define LOG_DEBUG(msg) Logger::getInstance().debug(msg)
#define LOG_WARNING(msg) Logger::getInstance().warning(msg)
