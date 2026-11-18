#pragma once

#include "FlexConsole.h"

// Log levels (same order as ESP-IDF)
#define DBUS2_LOG_LEVEL_NONE    0
#define DBUS2_LOG_LEVEL_ERROR   1
#define DBUS2_LOG_LEVEL_WARN    2
#define DBUS2_LOG_LEVEL_INFO    3
#define DBUS2_LOG_LEVEL_DEBUG   4
#define DBUS2_LOG_LEVEL_VERBOSE 5

// Global default level (override via -DDBUS2_LOG_DEFAULT_LEVEL=... in
// build_flags)
#ifndef DBUS2_LOG_DEFAULT_LEVEL
  #define DBUS2_LOG_DEFAULT_LEVEL DBUS2_LOG_LEVEL_INFO
#endif

// Per-file override (define before including this header)
#ifndef DBUS2_LOG_LOCAL_LEVEL
  #define DBUS2_LOG_LOCAL_LEVEL DBUS2_LOG_DEFAULT_LEVEL
#endif

// ANSI color toggle (override via -DDBUS2_LOG_COLOR_ENABLED=0 in build_flags)
#ifndef DBUS2_LOG_COLOR_ENABLED
  #define DBUS2_LOG_COLOR_ENABLED 1
#endif

#if DBUS2_LOG_COLOR_ENABLED
  #define _DBUS2_LOG_COLOR_E     "\033[31m"
  #define _DBUS2_LOG_COLOR_W     "\033[33m"
  #define _DBUS2_LOG_COLOR_I     "\033[32m"
  #define _DBUS2_LOG_COLOR_D     "\033[36m"
  #define _DBUS2_LOG_COLOR_V     ""
  #define _DBUS2_LOG_COLOR_RESET "\033[0m"
#else
  #define _DBUS2_LOG_COLOR_E     ""
  #define _DBUS2_LOG_COLOR_W     ""
  #define _DBUS2_LOG_COLOR_I     ""
  #define _DBUS2_LOG_COLOR_D     ""
  #define _DBUS2_LOG_COLOR_V     ""
  #define _DBUS2_LOG_COLOR_RESET ""
#endif

// Format: color prefix + reset before message text
// Output example: "I [DBus2] System online"
//                  ^^^^^^^^^  colored    ^^^^^ default
#define _DBUS2_LOG(level, letter, color, fmt, ...)                                                                     \
  do {                                                                                                                 \
    if (DBUS2_LOG_LOCAL_LEVEL >= (level)) {                                                                            \
      console.printf(color letter " [%s] " _DBUS2_LOG_COLOR_RESET fmt, TAG, ##__VA_ARGS__);                            \
      console.println();                                                                                               \
    }                                                                                                                  \
  } while (0)

#define DBUS2_LOGE(fmt, ...) _DBUS2_LOG(DBUS2_LOG_LEVEL_ERROR, "E", _DBUS2_LOG_COLOR_E, fmt, ##__VA_ARGS__)
#define DBUS2_LOGW(fmt, ...) _DBUS2_LOG(DBUS2_LOG_LEVEL_WARN, "W", _DBUS2_LOG_COLOR_W, fmt, ##__VA_ARGS__)
#define DBUS2_LOGI(fmt, ...) _DBUS2_LOG(DBUS2_LOG_LEVEL_INFO, "I", _DBUS2_LOG_COLOR_I, fmt, ##__VA_ARGS__)
#define DBUS2_LOGD(fmt, ...) _DBUS2_LOG(DBUS2_LOG_LEVEL_DEBUG, "D", _DBUS2_LOG_COLOR_D, fmt, ##__VA_ARGS__)
#define DBUS2_LOGV(fmt, ...) _DBUS2_LOG(DBUS2_LOG_LEVEL_VERBOSE, "V", _DBUS2_LOG_COLOR_V, fmt, ##__VA_ARGS__)
