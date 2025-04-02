cmake_minimum_required(VERSION 3.10)
project(sms_forward)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add static linking for Alpine
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

# Configure pkg-config for cross-compilation
find_package(PkgConfig REQUIRED)
set(ENV{PKG_CONFIG_PATH} "")
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/pkgconfig:/usr/share/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "/usr")

# Find required packages
pkg_check_modules(DBUS dbus-1)
if(NOT DBUS_FOUND)
    message(WARNING "dbus-1 not found, attempting manual configuration")
    set(DBUS_INCLUDE_DIRS
        "/usr/include/dbus-1.0"
        "/usr/lib/dbus-1.0/include"
        "/usr/include/libmm-glib"
        "/usr/include/ModemManager"
        "/usr/include/glib-2.0"
        "/usr/lib/aarch64-linux-gnu"
        "/usr/lib/aarch64-linux-gnu/glib-2.0/include"
        "/usr/lib/aarch64-linux-gnu/dbus-1.0/include"
    )
    set(DBUS_LIBRARY_DIRS "/usr/lib")
    set(DBUS_LIBRARIES "-ldbus-1")
endif()

pkg_check_modules(MM ModemManager)
if(NOT MM_FOUND)
    message(WARNING "ModemManager not found, attempting manual configuration")
    set(MM_INCLUDE_DIRS
        "/usr/include"
        "/usr/include/ModemManager"
        "/usr/include/libmm-glib"
        "/usr/include/glib-2.0"
        "/usr/lib/aarch64-linux-gnu"
        "/usr/lib/aarch64-linux-gnu/glib-2.0/include"
        "/usr/lib/aarch64-linux-gnu/ModemManager"
    )
    set(MM_LIBRARY_DIRS
        "/usr/lib"
        "/usr/lib/aarch64-linux-gnu"
    )
    # Use full paths to static libraries
    set(MM_LIBRARIES
        "/usr/lib/aarch64-linux-gnu/libmm-glib.so"
    )
endif()

pkg_check_modules(CURL libcurl)
if(NOT CURL_FOUND)
    message(WARNING "libcurl not found, attempting manual configuration")
    set(CURL_INCLUDE_DIRS "/usr/include")
    set(CURL_LIBRARY_DIRS "/usr/lib")
    set(CURL_LIBRARIES "-lcurl")
endif()

add_executable(sms_forward
    src/main.cpp
    src/sms_monitor.cpp
    src/wx_pusher.cpp
    src/config.cpp
    src/logger.cpp
)

target_include_directories(sms_forward PRIVATE
    /usr/include
    /usr/lib/dbus-1.0/include
    /usr/include/dbus-1.0
    ${DBUS_INCLUDE_DIRS}
    ${MM_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

link_directories(
    ${DBUS_LIBRARY_DIRS}
    ${MM_LIBRARY_DIRS}
    ${CURL_LIBRARY_DIRS}
)

target_link_libraries(sms_forward
    ${DBUS_LIBRARIES}
    ${MM_LIBRARIES}
    ${CURL_LIBRARIES}
    -lgio-2.0
    -lgobject-2.0
    -lgmodule-2.0
    -lglib-2.0
    -lpcre2-8
    -lpcre
    -lz
    -lffi
    -pthread
    -ldl
    -lresolv
)

# Add installation rules
install(TARGETS sms_forward DESTINATION bin)
install(FILES sms_forward.conf DESTINATION /etc)
