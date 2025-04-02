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
