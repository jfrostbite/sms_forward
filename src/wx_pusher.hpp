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
#include <curl/curl.h>

class WxPusher {
public:
    WxPusher(const std::string& token, const std::string& uid);
    ~WxPusher();

    // Send a message to WxPusher
    bool sendMessage(const std::string& title, const std::string& content);

private:
    std::string token;
    std::string uid;
    CURL* curl;
};
