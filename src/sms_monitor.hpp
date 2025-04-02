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
