# SMS Forward

A service that monitors incoming SMS messages and forwards them to WxPusher.

## Overview

SMS Forward is a C++ application that:
- Monitors incoming SMS messages using ModemManager
- Processes existing SMS messages when started
- Forwards these messages to WxPusher service
- Runs as a background service on Linux systems
- Implements smart handling to avoid duplicate message forwarding
- Uses a retry mechanism to ensure SMS content is fully received before processing

## Compilation

### Prerequisites

To compile this project, you need:
- CMake (version 3.10 or higher)
- C++ compiler with C++17 support
- Cross-compilation toolchain for ARM64 (if targeting Alpine Linux on ARM64)

### Compiling in ORB

This project can be compiled using ORB (OrbStack) with the following steps:

1. Enter the ORB environment:
   ```bash
   orb
   ```

2. Navigate to the project directory:
   ```bash
   cd /path/to/sms_forward
   ```

3. Create a build directory and run CMake:
   ```bash
   mkdir -p build
   cd build
   cmake .. -DCMAKE_TOOLCHAIN_FILE=../arm64-alpine-linux-toolchain.cmake
   ```

4. Build the project:
   ```bash
   make -j4
   ```

5. The compiled binary will be located at `build/sms_forward`

## Running in Alpine Linux

### Required Packages

To run the application in Alpine Linux, install the following packages:

```bash
# Update package list
apk update

# Install required packages
apk add dbus dbus-libs modemmanager modemmanager-libs glib libcurl curl
```

### Configuration

1. Create a configuration file at `/etc/sms_forward.conf`:
   ```
   # WxPusher configuration
   wx_pusher_token=AT_your_token_here
   wx_pusher_uid=UID_your_uid_here

   # Application behavior configuration
   forward_existing_sms=true
   only_forward_verification_codes=false
   debug_mode=false
   ```

   The configuration options are:

   **WxPusher configuration:**
   - `wx_pusher_token`: Your WxPusher application token (starts with AT_)
   - `wx_pusher_uid`: Your WxPusher user ID (starts with UID_)

   You can obtain these from the [WxPusher website](https://wxpusher.zjiecode.com)

   For more information about WxPusher API, see the [official documentation](https://wxpusher.zjiecode.com/docs/)

   **Application behavior configuration:**
   - `forward_existing_sms`: Whether to forward existing SMS messages at startup
     - `true` (default): Forward all existing SMS messages when the application starts
     - `false`: Only forward new SMS messages received after the application starts
   - `only_forward_verification_codes`: Whether to only forward verification code SMS messages
     - `false` (default): Forward all SMS messages
     - `true`: Only forward SMS messages that appear to contain verification codes
   - `debug_mode`: Whether to enable detailed debug logging
     - `false` (default): Only log essential information (INFO, WARNING, ERROR, FATAL)
     - `true`: Log detailed debug information (DEBUG level messages)

2. Ensure D-Bus and ModemManager services are running:
   ```bash
   # Start D-Bus service
   rc-service dbus start

   # Start ModemManager service
   rc-service modemmanager start

   # Make them start on boot
   rc-update add dbus
   rc-update add modemmanager
   ```

3. Create log directory:
   ```bash
   mkdir -p /var/log
   touch /var/log/sms_forward.log
   chmod 644 /var/log/sms_forward.log
   ```

### Installation

1. Copy the compiled executable to the system:
   ```bash
   cp sms_forward /usr/local/bin/
   chmod +x /usr/local/bin/sms_forward
   ```

2. Copy the configuration file:
   ```bash
   cp sms_forward.conf /etc/
   ```

### Running the Application

Run the application directly:
```bash
/usr/local/bin/sms_forward
```

### Creating a Service (Optional)

To run the application as a service:

1. Create a service file at `/etc/init.d/sms_forward`:
   ```bash
   #!/sbin/openrc-run

   name="SMS Forward Service"
   description="SMS Forwarding Service using ModemManager"
   command="/usr/local/bin/sms_forward"
   command_background="yes"
   pidfile="/run/${RC_SVCNAME}.pid"
   output_log="/var/log/sms_forward.out"
   error_log="/var/log/sms_forward.err"

   depend() {
       need net dbus modemmanager
   }
   ```

2. Make it executable and add it to startup:
   ```bash
   chmod +x /etc/init.d/sms_forward
   rc-update add sms_forward default
   ```

3. Start the service:
   ```bash
   rc-service sms_forward start
   ```

## Features and Implementation Details

### SMS Processing

The application implements several smart features to ensure reliable SMS forwarding:

1. **Configurable Message Processing**:
   - Option to process only new messages or include existing messages at startup
   - Controlled via the `forward_existing_sms` configuration option
   - Useful for avoiding re-processing of old messages when restarting the service
   - Option to only forward verification code SMS messages
   - Controlled via the `only_forward_verification_codes` configuration option
   - Useful for filtering out promotional and non-essential messages

2. **Smart Message Filtering**:
   - Only processes received SMS messages (ignores sent, draft, or unknown state messages)
   - Prevents crashes when manually created SMS messages are detected
   - ModemManager sometimes reports the same SMS in two storage locations (ME and SM)
   - The application filters messages by storage type to avoid duplicate forwarding
   - Only messages stored in the Mobile Equipment (ME) are processed

3. **Robust Error Handling and Debugging**:
   - Global exception handling to catch and log unhandled exceptions
   - Signal handlers to capture crash details (segmentation faults, etc.)
   - Detailed stack traces in logs for easier debugging
   - Graceful shutdown with informative error messages
   - Configurable debug mode for detailed logging
   - Easy to enable/disable debug output without code changes

4. **Delayed Processing**:
   - When a new SMS arrives, its content might not be immediately available
   - The application implements a retry mechanism with configurable delay
   - It attempts to read the SMS content multiple times before giving up
   - This ensures that SMS content is fully received before processing

5. **Fallback Mechanism**:
   - If the ModemManager API fails to provide SMS content after retries
   - The application falls back to using the `mmcli` command-line tool
   - This provides an additional layer of reliability

### WxPusher Integration

The application uses the WxPusher API to forward SMS messages:

1. **Secure Communication**:
   - Uses HTTPS for secure communication with the WxPusher API
   - Properly escapes special characters in JSON payloads

2. **Enhanced Message Display**:
   - Uses plain text formatting for maximum compatibility
   - Clear separation between sender, content, and timestamp
   - Verification codes are extracted and displayed in the summary field
   - Makes verification codes immediately visible in notifications

3. **Error Handling**:
   - Detailed error logging for API responses
   - Retry mechanism for transient failures

## Troubleshooting

If you encounter issues:

1. Check library dependencies:
   ```bash
   ldd /usr/local/bin/sms_forward
   ```

2. Verify ModemManager can detect your modem:
   ```bash
   mmcli -L
   ```

3. Check application logs:
   ```bash
   cat /var/log/sms_forward.log
   ```

4. Ensure proper D-Bus permissions:
   ```bash
   # If running as non-root, add your user to the messagebus group
   adduser yourusername messagebus
   ```

5. Check for duplicate SMS messages:
   ```bash
   # List all SMS messages
   mmcli -m 0 --messaging-list-sms

   # View details of a specific SMS
   mmcli -s <sms_index>
   ```

   If you see duplicate messages with different storage types (ME and SM), this is normal.
   The application is designed to handle this and only process messages stored in ME (Mobile Equipment).

6. If SMS content is not being forwarded:
   - Check if there's a delay in the ModemManager processing the SMS
   - The application implements a retry mechanism with a delay
   - You can increase the retry count or delay in the source code if needed

7. If the application crashes with a segmentation fault:
   - Check if you're manually creating SMS messages with `mmcli`
   - The application now filters out non-received SMS messages to prevent crashes
   - Only SMS messages with state `MM_SMS_STATE_RECEIVED` are processed
   - The application has robust error handling to prevent crashes from regex or other exceptions
   - Check the logs for detailed error messages that can help diagnose the issue
   - The application now captures crash details and stack traces in the log file
   - Look for log entries with level `FATAL` for crash information
   - Example crash log:
     ```
     [2025-04-02 21:30:00][FATAL] Program crashed with signal: SIGSEGV (Segmentation fault)
     [2025-04-02 21:30:00][FATAL] Stack trace:
     #0: /usr/local/bin/sms_forward(+0x12345) [0x555555554345]
     #1: /usr/local/bin/sms_forward(+0x23456) [0x555555565456]
     #2: /lib/aarch64-linux-gnu/libc.so.6(+0x34567) [0x7ffff7bc4567]
     ```

8. If you need more detailed logs for troubleshooting:
   - Enable debug mode in the configuration file:
     ```
     debug_mode=true
     ```
   - Restart the application
   - Check the log file for detailed DEBUG level messages
   - This will show detailed information about SMS processing, API calls, etc.
   - Remember to disable debug mode after troubleshooting to reduce log file size



## Customization

You can customize the application by modifying the source code:

### Adjusting Retry Parameters

In `src/sms_monitor.cpp`, you can modify the retry parameters for SMS content retrieval:

```cpp
// Try to get text and number with retries
const char* text = nullptr;
const char* number = nullptr;
int max_retries = 5;        // Increase for more retries
int retry_delay_ms = 1000;  // Adjust delay between retries (milliseconds)
```

### Changing Message Format

In `src/main.cpp`, you can modify the message format sent to WxPusher:

```cpp
monitor.setCallback([&pusher](const std::string& sender, const std::string& content) {
    // Customize the message format here
    bool result = pusher.sendMessage("New SMS from " + sender, content);
});
```

### Adding More Logging

You can add more logging by using the logging macros:

```cpp
LOG_DEBUG("Debug message");
LOG_INFO("Info message");
LOG_WARNING("Warning message");
LOG_ERROR("Error message");
```

### Customizing Verification Code Detection

You can modify the verification code detection logic in `src/main.cpp`:

```cpp
bool isVerificationCode(const std::string& message) {
    // Common verification code patterns in Chinese SMS messages
    static const std::vector<std::regex> patterns = {
        std::regex("[验證]证码[：:][\\s]*([0-9]{4,6})"),  // Verification code: 123456
        // Add your own patterns here
    };

    // Check for common verification code keywords
    static const std::vector<std::string> keywords = {
        "验证码", "验证碼", "校验码", "校验碼", "动态码", "动态碼",
        // Add your own keywords here
    };

    // ...
}
```

## License

### Proprietary License

Copyright (c) 2025 J.Kev.Fen

All rights reserved.

This software and associated documentation files (the "Software") are the proprietary property of the copyright holder. You are granted a limited, non-transferable right to use the Software solely for personal, non-commercial purposes.

**Restrictions:**

1. You may NOT modify, adapt, or create derivative works based on the Software.
2. You may NOT use the Software for commercial purposes.
3. You may NOT distribute, sublicense, or make the Software available to any third party.
4. You may NOT reverse engineer, decompile, or disassemble the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
