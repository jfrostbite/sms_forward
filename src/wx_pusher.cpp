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

#include "wx_pusher.hpp"
#include "logger.hpp"
#include <string>
#include <sstream>
#include <curl/curl.h>

// Callback function to capture response data
static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* response = (std::string*)userp;
    response->append((char*)contents, realsize);
    return realsize;
}

WxPusher::WxPusher(const std::string& token, const std::string& uid)
    : token(token), uid(uid) {
    curl = curl_easy_init();
}

WxPusher::~WxPusher() {
    if (curl) curl_easy_cleanup(curl);
}

bool WxPusher::sendMessage(const std::string& title, const std::string& content) {
    if (!curl) {
        LOG_ERROR("CURL not initialized");
        return false;
    }

    // Create a properly escaped JSON string
    std::string jsonContent = title + "\n" + content;

    // Replace problematic characters in JSON
    auto escapeJson = [](std::string str) {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                default: result += c;
            }
        }
        return result;
    };

    std::string escapedToken = escapeJson(token);
    std::string escapedContent = escapeJson(jsonContent);
    std::string escapedUid = escapeJson(uid);
    std::string escapedSummary = escapeJson(title);

    // Build the JSON payload properly
    std::stringstream json;
    json << "{";
    json << "\"appToken\":\"" << escapedToken << "\"";
    json << ",\"content\":\"" << escapedContent << "\"";
    json << ",\"uids\":[\"" << escapedUid << "\"]";
    json << ",\"summary\":\"" << escapedSummary << "\"";
    json << "}";

    std::string jsonStr = json.str();
    LOG_DEBUG("Sending to WxPusher: " + jsonStr);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Use HTTPS instead of HTTP
    curl_easy_setopt(curl, CURLOPT_URL, "https://wxpusher.zjiecode.com/api/send/message");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    // Capture the response
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Set SSL verification options
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        LOG_ERROR("Failed to send message to WxPusher: " + std::string(curl_easy_strerror(res)));
        return false;
    }

    // Log the response
    LOG_DEBUG("WxPusher API response: " + response);

    // Check if the response contains success
    if (response.find("\"success\":true") != std::string::npos) {
        LOG_INFO("Message sent to WxPusher successfully");
        return true;
    } else {
        LOG_ERROR("WxPusher API returned error: " + response);
        return false;
    }
}
