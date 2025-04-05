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

#include "config.hpp"
#include <fstream>
#include <sstream>

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

bool Config::load(const std::string& config_path) {
    std::ifstream file(config_path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string key, value;

        if (std::getline(iss, key, '=') && std::getline(iss, value)) {
            // Trim whitespace from key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            if (key == "wx_pusher_token") wx_pusher_token = value;
            else if (key == "wx_pusher_uid") wx_pusher_uid = value;
            else if (key == "forward_existing_sms") {
                // Convert string to boolean
                forward_existing_sms = !(value == "false" || value == "0" || value == "no");
            }
            else if (key == "only_forward_verification_codes") {
                // Convert string to boolean
                only_forward_verification_codes = (value == "true" || value == "1" || value == "yes");
            }
            else if (key == "debug_mode") {
                // Convert string to boolean
                debug_mode = (value == "true" || value == "1" || value == "yes");
            }
            else if (key == "delete_after_forwarding") {
                // Convert string to boolean
                delete_after_forwarding = (value == "true" || value == "1" || value == "yes");
            }
        }
    }

    return !wx_pusher_token.empty() && !wx_pusher_uid.empty();
}
