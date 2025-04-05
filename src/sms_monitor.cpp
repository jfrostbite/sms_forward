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
#include "logger.hpp"
#include <stdexcept>
#include <ModemManager.h>
#include <libmm-glib.h>
#include <glib.h>
#include <gio/gio.h>
#include <time.h>  // For nanosleep

SmsMonitor::SmsMonitor() : connection(nullptr) {}

SmsMonitor::~SmsMonitor() {
    if (connection) {
        dbus_connection_unref(connection);
    }
}

bool SmsMonitor::init() {
    DBusError error;
    dbus_error_init(&error);

    connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (!connection) {
        LOG_ERROR("Failed to connect to system bus: " + std::string(error.message));
        dbus_error_free(&error);
        return false;
    }

    dbus_bus_add_match(connection,
        "type='signal',interface='org.freedesktop.ModemManager1.Modem.Messaging'",
        &error);

    if (dbus_error_is_set(&error)) {
        LOG_ERROR("Failed to add match: " + std::string(error.message));
        dbus_error_free(&error);
        return false;
    }

    LOG_INFO("SMS Monitor initialized successfully");
    return true;
}

void SmsMonitor::setCallback(SmsCallback cb) {
    callback = std::move(cb);
}

void SmsMonitor::run() {
    while (dbus_connection_read_write_dispatch(connection, -1)) {
        DBusMessage* msg = dbus_connection_pop_message(connection);
        if (msg) {
            handleMessage(msg, this);
            dbus_message_unref(msg);
        }
    }
}

void SmsMonitor::processSms(MMSms* sms) {
    LOG_DEBUG("processSms called");

    if (!sms) {
        LOG_ERROR("processSms: SMS is null");
        return;
    }

    if (!callback) {
        LOG_ERROR("processSms: Callback is not set");
        return;
    }

    // Get the SMS path for retry identification
    const char* sms_path = mm_sms_get_path(sms);
    std::string path_str = sms_path ? std::string(sms_path) : "unknown";

    // Check the SMS state to only process received messages
    MMSmsState state = mm_sms_get_state(sms);
    LOG_DEBUG("SMS state: " + std::to_string(state));

    // Only process received SMS messages
    if (state != MM_SMS_STATE_RECEIVED) {
        LOG_DEBUG("Skipping SMS with state " + std::to_string(state) + " (not received)");
        return;
    }

    // Check the storage type to avoid duplicate processing
    MMSmsStorage storage = mm_sms_get_storage(sms);
    LOG_DEBUG("SMS storage type: " + std::to_string(storage));

    // Only process SMS messages with storage type MM_SMS_STORAGE_ME (ME = mobile equipment)
    // Skip MM_SMS_STORAGE_SM (SM = SIM card) to avoid duplicates
    if (storage != MM_SMS_STORAGE_ME) {
        LOG_DEBUG("Skipping SMS with storage type " + std::to_string(storage) + " (only processing ME storage)");
        return;
    }

    // Try to get text and number with retries
    const char* text = nullptr;
    const char* number = nullptr;
    int max_retries = 5;
    int retry_delay_ms = 1000; // 1 second

    for (int retry = 0; retry < max_retries; retry++) {
        text = mm_sms_get_text(sms);
        number = mm_sms_get_number(sms);

        LOG_DEBUG("processSms [" + path_str + "] retry " + std::to_string(retry) + ": text=" +
                 (text ? std::string(text) : "null") + ", number=" +
                 (number ? std::string(number) : "null"));

        if (text && number) {
            break; // We have both text and number, no need to retry
        }

        if (retry < max_retries - 1) {
            LOG_DEBUG("Waiting for SMS to be fully received, retrying in " +
                     std::to_string(retry_delay_ms) + "ms...");

            // Sleep for retry_delay_ms milliseconds
            struct timespec ts;
            ts.tv_sec = retry_delay_ms / 1000;
            ts.tv_nsec = (retry_delay_ms % 1000) * 1000000;
            nanosleep(&ts, nullptr);
        }
    }

    if (text && number) {
        LOG_INFO("SMS from: " + std::string(number));
        LOG_DEBUG("SMS content: " + std::string(text));

        LOG_DEBUG("Calling callback with number=" + std::string(number) + ", text=" + std::string(text));
        callback(number, text);
        LOG_DEBUG("Callback completed");
    } else {
        LOG_ERROR("processSms: Text or number is still null after " +
                 std::to_string(max_retries) + " retries");

        // Try to get the SMS directly using mmcli as a last resort
        if (sms_path) {
            // Extract the SMS index from the path
            const char* sms_index_str = strrchr(sms_path, '/');
            if (sms_index_str) {
                sms_index_str++; // Skip the '/' character
                int sms_index = atoi(sms_index_str);
                LOG_DEBUG("Attempting to get SMS content using mmcli for SMS index " +
                         std::to_string(sms_index));

                // Get the SMS content using mmcli
                std::string cmd = "mmcli -m 0 --sms=" + std::to_string(sms_index);
                FILE* pipe = popen(cmd.c_str(), "r");
                if (pipe) {
                    char buffer[1024];
                    std::string result = "";
                    while (!feof(pipe)) {
                        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                            result += buffer;
                        }
                    }
                    pclose(pipe);

                    LOG_DEBUG("mmcli output: " + result);

                    // Parse the output to get the SMS content and number
                    std::string parsed_number = "";
                    std::string parsed_text = "";

                    // Extract number
                    size_t number_pos = result.find("number:");
                    if (number_pos != std::string::npos) {
                        size_t start = result.find('\'', number_pos);
                        size_t end = result.find('\'', start + 1);
                        if (start != std::string::npos && end != std::string::npos) {
                            parsed_number = result.substr(start + 1, end - start - 1);
                        }
                    }

                    // Extract text
                    size_t text_pos = result.find("text:");
                    if (text_pos != std::string::npos) {
                        size_t start = result.find('\'', text_pos);
                        size_t end = result.find('\'', start + 1);
                        if (start != std::string::npos && end != std::string::npos) {
                            parsed_text = result.substr(start + 1, end - start - 1);
                        }
                    }

                    if (!parsed_number.empty() && !parsed_text.empty()) {
                        LOG_INFO("Successfully extracted SMS content using mmcli");
                        LOG_INFO("SMS from: " + parsed_number);
                        LOG_DEBUG("SMS content: " + parsed_text);

                        // Call the callback directly
                        callback(parsed_number.c_str(), parsed_text.c_str());
                    }
                }
            }
        }
    }
}

void SmsMonitor::checkExistingSms() {
    if (!callback) {
        LOG_WARNING("No callback set, skipping existing SMS check");
        return;
    }

    LOG_INFO("Checking for existing SMS messages...");

    GError* error = nullptr;
    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
    if (error) {
        LOG_ERROR("Failed to get GDBus connection: " + std::string(error->message));
        g_error_free(error);
        return;
    }

    // Create ModemManager Manager proxy
    MMManager* manager = mm_manager_new_sync(
        connection,
        G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
        nullptr,  // cancellable
        &error
    );

    if (error) {
        LOG_ERROR("Failed to create ModemManager proxy: " + std::string(error->message));
        g_error_free(error);
        g_object_unref(connection);
        return;
    }

    // Get all modems
    GList* modems = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(manager));
    if (!modems) {
        LOG_WARNING("No modems found");
        g_object_unref(manager);
        g_object_unref(connection);
        return;
    }

    int processed_count = 0;

    // Process each modem
    for (GList* m = modems; m; m = g_list_next(m)) {
        MMObject* modem_obj = MM_OBJECT(m->data);
        MMModemMessaging* messaging = mm_object_get_modem_messaging(modem_obj);

        if (!messaging) {
            LOG_DEBUG("Modem does not support messaging");
            continue;
        }

        // List SMS messages
        GList* sms_list = mm_modem_messaging_list_sync(messaging, nullptr, &error);
        if (error) {
            LOG_ERROR("Failed to get SMS list: " + std::string(error->message));
            g_error_free(error);
            error = nullptr;
            g_object_unref(messaging);
            continue;
        }

        // Process each SMS
        for (GList* l = sms_list; l; l = g_list_next(l)) {
            MMSms* sms = MM_SMS(l->data);
            MMSmsState state = mm_sms_get_state(sms);

            // Only process received messages
            if (state == MM_SMS_STATE_RECEIVED) {
                processSms(sms);
                processed_count++;
            }
        }

        // Cleanup
        g_list_free_full(sms_list, g_object_unref);
        g_object_unref(messaging);
    }

    LOG_INFO("Processed " + std::to_string(processed_count) + " existing SMS messages");

    // Cleanup
    g_list_free_full(modems, g_object_unref);
    g_object_unref(manager);
    g_object_unref(connection);
}

void SmsMonitor::handleMessage(DBusMessage* message, void* user_data) {
    auto* monitor = static_cast<SmsMonitor*>(user_data);

    if (dbus_message_is_signal(message,
        "org.freedesktop.ModemManager1.Modem.Messaging", "Added")) {

        DBusMessageIter args;
        if (!dbus_message_iter_init(message, &args)) {
            return;
        }

        const char* path = nullptr;
        dbus_message_iter_get_basic(&args, &path);
        if (!path) return;

        GError* error = nullptr;
        GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, &error);
        if (error) {
            LOG_ERROR("Failed to get GDBus connection: " + std::string(error->message));
            g_error_free(error);
            return;
        }

        // Create ModemManager Manager proxy
        MMManager* manager = mm_manager_new_sync(
            connection,
            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
            nullptr,  // cancellable
            &error
        );

        if (error) {
            LOG_ERROR("Failed to create ModemManager proxy: " + std::string(error->message));
            g_error_free(error);
            g_object_unref(connection);
            return;
        }

        // The path in the signal could be either the SMS path or the modem path
        LOG_DEBUG("Received SMS signal with path: " + std::string(path));

        // Check if the path is an SMS path
        bool is_sms_path = strstr(path, "/SMS/") != nullptr;

        if (is_sms_path) {
            // This is an SMS path, we need to find the modem that owns this SMS
            // Get all modems
            GList* objects = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(manager));
            if (!objects) {
                LOG_ERROR("Failed to get modem objects");
                g_object_unref(manager);
                g_object_unref(connection);
                return;
            }

            LOG_DEBUG("Found " + std::to_string(g_list_length(objects)) + " modem objects");

            // Process each modem to find the SMS
            bool found_sms = false;
            int modem_count = 0;

            for (GList* l = objects; l && !found_sms; l = g_list_next(l)) {
                modem_count++;
                MMObject* modem = MM_OBJECT(l->data);
                const char* modem_path = g_dbus_object_get_object_path(G_DBUS_OBJECT(modem));
                LOG_DEBUG("Checking modem " + std::to_string(modem_count) + ": " + std::string(modem_path));

                MMModemMessaging* messaging = mm_object_get_modem_messaging(modem);
                if (!messaging) {
                    LOG_DEBUG("Modem does not support messaging");
                    continue;
                }

                // List SMS messages
                GList* sms_list = mm_modem_messaging_list_sync(messaging, nullptr, &error);
                if (error) {
                    LOG_ERROR("Failed to get SMS list: " + std::string(error->message));
                    g_error_free(error);
                    error = nullptr;
                    g_object_unref(messaging);
                    continue;
                }

                LOG_DEBUG("Found " + std::to_string(g_list_length(sms_list)) + " SMS messages");

                // Find the SMS with the matching path
                int sms_count = 0;
                for (GList* s = sms_list; s && !found_sms; s = g_list_next(s)) {
                    sms_count++;
                    MMSms* sms = MM_SMS(s->data);
                    const char* sms_path = mm_sms_get_path(sms);

                    if (sms_path) {
                        LOG_DEBUG("SMS " + std::to_string(sms_count) + " path: " + std::string(sms_path));

                        if (strcmp(sms_path, path) == 0) {
                            LOG_DEBUG("Found matching SMS path");
                            // Found the SMS, process it
                            MMSmsState state = mm_sms_get_state(sms);
                            LOG_DEBUG("SMS state: " + std::to_string(state));

                            // Only process received SMS messages
                            if (state == MM_SMS_STATE_RECEIVED) {
                                monitor->processSms(sms);
                                found_sms = true;
                            } else {
                                LOG_DEBUG("Skipping SMS with state " + std::to_string(state) + " (not received)");
                            }
                        }
                    } else {
                        LOG_DEBUG("SMS " + std::to_string(sms_count) + " has no path");
                    }
                }

                // Cleanup
                g_list_free_full(sms_list, g_object_unref);
                g_object_unref(messaging);
            }

            // Cleanup
            g_list_free_full(objects, g_object_unref);

            if (!found_sms) {
                LOG_ERROR("Failed to find SMS with path: " + std::string(path));

                // Try a different approach - get the SMS directly from the path
                LOG_DEBUG("Trying alternative approach to get SMS");

                // Extract the modem index and SMS index from the path
                // Format: /org/freedesktop/ModemManager1/SMS/22
                const char* sms_index_str = strrchr(path, '/');
                if (sms_index_str) {
                    sms_index_str++; // Skip the '/' character
                    int sms_index = atoi(sms_index_str);
                    LOG_DEBUG("SMS index: " + std::to_string(sms_index));

                    // Get the first modem
                    objects = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(manager));
                    if (objects && objects->data) {
                        MMObject* modem = MM_OBJECT(objects->data);
                        const char* modem_path = g_dbus_object_get_object_path(G_DBUS_OBJECT(modem));
                        LOG_DEBUG("Using first modem: " + std::string(modem_path));

                        // Try to get the SMS directly using mmcli command
                        LOG_DEBUG("Attempting to process SMS using mmcli command");

                        // Get the SMS content using mmcli
                        std::string cmd = "mmcli -m 0 --sms=" + std::to_string(sms_index);
                        FILE* pipe = popen(cmd.c_str(), "r");
                        if (pipe) {
                            char buffer[1024];
                            std::string result = "";
                            while (!feof(pipe)) {
                                if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                                    result += buffer;
                                }
                            }
                            pclose(pipe);

                            LOG_DEBUG("mmcli output: " + result);

                            // Parse the output to get the SMS content and number
                            std::string number = "";
                            std::string text = "";

                            // Extract number
                            size_t number_pos = result.find("number:");
                            if (number_pos != std::string::npos) {
                                size_t start = result.find('\'', number_pos);
                                size_t end = result.find('\'', start + 1);
                                if (start != std::string::npos && end != std::string::npos) {
                                    number = result.substr(start + 1, end - start - 1);
                                }
                            }

                            // Extract text
                            size_t text_pos = result.find("text:");
                            if (text_pos != std::string::npos) {
                                size_t start = result.find('\'', text_pos);
                                size_t end = result.find('\'', start + 1);
                                if (start != std::string::npos && end != std::string::npos) {
                                    text = result.substr(start + 1, end - start - 1);
                                }
                            }

                            if (!number.empty() && !text.empty()) {
                                LOG_INFO("Successfully extracted SMS content using mmcli");
                                LOG_INFO("SMS from: " + number);
                                LOG_DEBUG("SMS content: " + text);

                                // Call the callback directly
                                if (monitor->callback) {
                                    monitor->callback(number, text);
                                }

                                found_sms = true;
                            }
                        }

                        g_list_free_full(objects, g_object_unref);
                    }
                }
            }
        } else {
            // This is a modem path, get all SMS messages from this modem
            // Get all modems
            GList* objects = g_dbus_object_manager_get_objects(G_DBUS_OBJECT_MANAGER(manager));
            if (!objects) {
                LOG_ERROR("Failed to get modem objects");
                g_object_unref(manager);
                g_object_unref(connection);
                return;
            }

            // Find the modem that sent the signal
            MMObject* modem = nullptr;
            for (GList* l = objects; l; l = g_list_next(l)) {
                MMObject* obj = MM_OBJECT(l->data);
                const char* obj_path = g_dbus_object_get_object_path(G_DBUS_OBJECT(obj));

                if (strcmp(obj_path, path) == 0) {
                    modem = obj;
                    break;
                }
            }

            if (!modem) {
                LOG_ERROR("Failed to find modem with path: " + std::string(path));
                g_list_free_full(objects, g_object_unref);
                g_object_unref(manager);
                g_object_unref(connection);
                return;
            }

            // Get the messaging interface
            MMModemMessaging* messaging = mm_object_get_modem_messaging(modem);
            if (!messaging) {
                LOG_ERROR("Failed to get messaging interface");
                g_list_free_full(objects, g_object_unref);
                g_object_unref(manager);
                g_object_unref(connection);
                return;
            }

            // List SMS messages
            GList* sms_list = mm_modem_messaging_list_sync(messaging, nullptr, &error);
            if (error) {
                LOG_ERROR("Failed to get SMS list: " + std::string(error->message));
                g_error_free(error);
                g_object_unref(messaging);
                g_list_free_full(objects, g_object_unref);
                g_object_unref(manager);
                g_object_unref(connection);
                return;
            }

            // Process all SMS messages
            for (GList* l = sms_list; l; l = g_list_next(l)) {
                MMSms* sms = MM_SMS(l->data);
                MMSmsState state = mm_sms_get_state(sms);

                // Only process received messages
                if (state == MM_SMS_STATE_RECEIVED) {
                    monitor->processSms(sms);
                }
            }

            // Cleanup
            g_list_free_full(sms_list, g_object_unref);
            g_object_unref(messaging);
            g_list_free_full(objects, g_object_unref);
        }

        // Cleanup
        g_object_unref(manager);
        g_object_unref(connection);
    }
}
