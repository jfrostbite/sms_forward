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

class Config {
public:
    static Config& getInstance();
    bool load(const std::string& config_path);

    std::string getWxPusherToken() const { return wx_pusher_token; }
    std::string getWxPusherUid() const { return wx_pusher_uid; }
    bool getForwardExistingSms() const { return forward_existing_sms; }
    bool getOnlyForwardVerificationCodes() const { return only_forward_verification_codes; }
    bool getDebugMode() const { return debug_mode; }

private:
    Config() : forward_existing_sms(true), only_forward_verification_codes(false), debug_mode(false) {} // Default values for backward compatibility
    std::string wx_pusher_token;
    std::string wx_pusher_uid;
    bool forward_existing_sms; // Whether to forward existing SMS messages at startup
    bool only_forward_verification_codes; // Whether to only forward verification code SMS messages
    bool debug_mode; // Whether to enable debug logging
};
