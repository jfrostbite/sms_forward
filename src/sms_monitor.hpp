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
#include <dbus/dbus.h>
#include <string>
#include <functional>
#include <libmm-glib.h>

class SmsMonitor {
public:
    using SmsCallback = std::function<void(const std::string&, const std::string&)>;

    SmsMonitor();
    ~SmsMonitor();

    bool init();
    void setCallback(SmsCallback callback);
    void run();
    void checkExistingSms();

private:
    DBusConnection* connection;
    SmsCallback callback;
    static void handleMessage(DBusMessage* message, void* user_data);
    void processSms(MMSms* sms);
};
