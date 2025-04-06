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

#include "sms_monitor.hpp"
#include "wx_pusher.hpp"
#include "config.hpp"
#include "logger.hpp"
#include <iostream>
#include <regex>

// Function to check if a message contains a verification code
bool isVerificationCode(const std::string& message) {
    try {
        if (message.empty()) {
            LOG_WARNING("Empty message passed to isVerificationCode");
            return false;
        }

        // Simple check for common verification code keywords before using regex
        // This is faster and less prone to errors
        if (message.find("\u9a8c\u8bc1\u7801") != std::string::npos ||
            message.find("code") != std::string::npos ||
            message.find("Code") != std::string::npos) {
            return true;
        }

        // Check for common verification code keywords
        try {
            static const std::vector<std::string> keywords = {
                "\u9a8c\u8bc1\u7801", "\u9a8c\u8bc1\u78bc", "\u6821\u9a8c\u7801", "\u6821\u9a8c\u78bc", "\u52a8\u6001\u7801", "\u52a8\u6001\u78bc",
                "\u786e\u8ba4\u7801", "\u78ba\u8a8d\u78bc", "\u77ed\u4fe1\u7801", "\u77ed\u4fe1\u78bc", "code", "Code", "CODE"
            };

            for (const auto& keyword : keywords) {
                try {
                    if (message.find(keyword) != std::string::npos) {
                        // Look for 4-6 digit numbers in the message
                        try {
                            std::regex digit_pattern("[0-9]{4,6}");
                            std::smatch match;
                            if (std::regex_search(message, match, digit_pattern)) {
                                return true;
                            }
                        } catch (const std::exception& e) {
                            LOG_ERROR("Exception in digit pattern regex: " + std::string(e.what()));
                            // If regex fails, assume it's a verification code
                            return true;
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception in keyword find: " + std::string(e.what()));
                    // Continue with next keyword
                }
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception in keywords initialization: " + std::string(e.what()));
        }

        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception in isVerificationCode: " + std::string(e.what()));
        return false;
    } catch (...) {
        LOG_ERROR("Unknown exception in isVerificationCode");
        return false;
    }
}

int main(int argc, char* argv[]) {
    try {
        if (!Logger::getInstance().init("/var/log/sms_forward.log")) {
            std::cerr << "Failed to initialize logger" << std::endl;
            return 1;
        }

        LOG_INFO("SMS Forward service starting...");

        // 显示调试模式状态
        if (Config::getInstance().getDebugMode()) {
            LOG_INFO("Debug mode enabled");
        }

        // 设置全局异常处理
        std::set_terminate([]() {
            try {
                // 重新抛出当前异常
                std::exception_ptr eptr = std::current_exception();
                if (eptr) {
                    std::rethrow_exception(eptr);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Unhandled exception: " + std::string(e.what()));
            } catch (...) {
                LOG_ERROR("Unknown unhandled exception");
            }

            // 记录调用栈
            LOG_ERROR("Stack trace:\n" + Logger::getInstance().getStackTrace());

            // 终止程序
            abort();
        });

        if (!Config::getInstance().load("/etc/sms_forward.conf")) {
            LOG_ERROR("Failed to load config");
            return 1;
        }

        WxPusher pusher(
            Config::getInstance().getWxPusherToken(),
            Config::getInstance().getWxPusherUid()
        );

        SmsMonitor monitor;
        if (!monitor.init()) {
            std::cerr << "Failed to init SMS monitor" << std::endl;
            return 1;
        }

        monitor.setCallback([&pusher, &monitor](const std::string& sender, const std::string& content, MMSms* sms) {
            try {
                LOG_DEBUG("Callback invoked with sender=" + sender + ", content=" + content);

                // Check if we should only forward verification codes
                if (Config::getInstance().getOnlyForwardVerificationCodes()) {
                    // Check if we should only forward verification codes and extract the code if present
                    bool is_verification = false;
                    std::string verification_code = "";

                    try {
                        is_verification = isVerificationCode(content);
                        LOG_DEBUG("Verification code check: " + std::string(is_verification ? "true" : "false"));
                    } catch (const std::exception& e) {
                        LOG_ERROR("Exception in verification code check: " + std::string(e.what()));
                        // Default to forwarding the message if verification check fails
                        is_verification = true;
                    }

                    // Skip non-verification code messages if configured to do so
                    if (Config::getInstance().getOnlyForwardVerificationCodes() && !is_verification) {
                        LOG_INFO("Skipping non-verification code SMS from " + sender);
                        return;
                    }
                }

                bool forwarding_success = false;
                try {
                    forwarding_success = pusher.sendMessage("New SMS from " + sender, content);
                    LOG_DEBUG("WxPusher sendMessage result: " + std::string(forwarding_success ? "success" : "failure"));

                    // Only delete SMS if forwarding was successful and deletion is enabled
                    if (forwarding_success && Config::getInstance().getDeleteAfterForwarding() && sms != nullptr) {
                        if (monitor.deleteSms(sms)) {
                            LOG_INFO("SMS from " + sender + " deleted after successful forwarding");
                        } else {
                            LOG_ERROR("Failed to delete SMS from " + sender + " after forwarding");
                        }
                    }
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception in WxPusher sendMessage: " + std::string(e.what()));
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Exception in SMS callback: " + std::string(e.what()));
            } catch (...) {
                LOG_ERROR("Unknown exception in SMS callback");
            }
        });

        // Check for existing SMS messages after callback is set (if enabled in config)
        if (Config::getInstance().getForwardExistingSms()) {
            LOG_INFO("Checking for existing SMS messages (enabled in config)");
            monitor.checkExistingSms();
        } else {
            LOG_INFO("Skipping existing SMS messages (disabled in config)");
        }

        LOG_INFO("SMS Forward service started successfully");
        std::cout << "SMS Forward started" << std::endl;
        monitor.run();

        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("Unhandled exception in main: " + std::string(e.what()));
        LOG_ERROR("Stack trace:\n" + Logger::getInstance().getStackTrace());
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        LOG_ERROR("Unknown unhandled exception in main");
        LOG_ERROR("Stack trace:\n" + Logger::getInstance().getStackTrace());
        std::cerr << "Fatal error: Unknown exception" << std::endl;
        return 1;
    }
}
