/**
    @file log.h
    @brief Process Watchdog Application Manager

    The Process Watchdog application manages the processes listed in the configuration file.
    It listens to a specified UDP port for heartbeat messages from these processes, which must
    periodically send their PID. If any process stops running or fails to send its PID over UDP
    within the expected interval, the Process Watchdog application will restart the process.

    The application ensures high reliability and availability by continuously monitoring and
    restarting processes as necessary. It also logs various statistics about the monitored
    processes, including start times, crash times, and heartbeat intervals.

    @date 2023-01-01
    @version 1.0
    @author by Eray Ozturk | erayozturk1@gmail.com
    @url github.com/diffstorm
    @license GPL-3 License
*/

#ifndef LOG_H
#define LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
    @file log.h
    @brief Logging system with syslog, file or stdout/stderr output.
*/

typedef enum
{
    LOG_EMERG = 0,    /* system is unusable */
    LOG_ALERT,        /* action must be taken immediately */
    LOG_CRIT,         /* critical conditions */
    LOG_ERR,          /* error conditions */
    LOG_WARNING,      /* warning conditions */
    LOG_NOTICE,       /* normal but significant condition */
    LOG_INFO,         /* informational */
    LOG_DEBUG,        /* debug-level messages */
    LOG_PRIORITY_MAX
} log_priority_t;

// Text colors
#define RED     "\x1b[31m"
#define GREEN   "\x1b[32m"
#define YELLOW  "\x1b[33m"
#define BLUE    "\x1b[34m"
#define MAGENTA "\x1b[35m"
#define CYAN    "\x1b[36m"
#define RESET   "\x1b[0m"

// Debug log section
// -----------------------------------------------------------------------------
// - Master switch
#define DEBUG_LOG               1   // 1 : enable | 0 : disable

#define SYSLOG_LOG              0   // 1 : enable | 0 : disable

// - Level switches
#define DEBUG_LOG_LEVEL_ERROR   1   // 1 : enable | 0 : disable
#define DEBUG_LOG_LEVEL_WARNING 1   // 1 : enable | 0 : disable
#define DEBUG_LOG_LEVEL_NOTICE  1   // 1 : enable | 0 : disable
#define DEBUG_LOG_LEVEL_INFO    0   // 1 : enable | 0 : disable
#define DEBUG_LOG_LEVEL_DEBUG   0   // 1 : enable | 0 : disable
#define DEBUG_LOG_LEVEL_FILE    1   // 1 : enable | 0 : disable

#define SYSLOG_LOG_LEVEL        LOG_NOTICE
#define FILE_LOG_LEVEL          LOG_NOTICE
#define FILE_LOG_SIZE_MAX       (100 * 1024) // [bytes]
#define DEBUG_LOG_FILENAME      "wdt.log"
#define DEBUG_LOG_OLD_FILENAME  "wdt.old.log"

#define DEBUG_LOG_TABLE_VIEW    1   // 1 : enable | 0 : disable

#ifndef __func__
//#define __func__ __FUNCTION__
#endif

#define __S1(x) #x
#define __S2(x) __S1(x)
#if DEBUG_LOG_TABLE_VIEW
#define __LOCATION __FILE__ ":" __S2(__LINE__)
#else
#define __LOCATION " @ " __FILE__ " : " __S2(__LINE__)
#endif

void iLOG(const char *function, const char *location, log_priority_t type, const char *format, ...);

// LOG Levels
// -----------------------------------------------------------------------------
#if DEBUG_LOG && DEBUG_LOG_LEVEL_ERROR
#define LOGE(...)  iLOG((const char *)__func__, (const char *)__LOCATION, LOG_ERR, __VA_ARGS__)
#else
#define LOGE(...)  ((void)0)
#endif

#if DEBUG_LOG && DEBUG_LOG_LEVEL_WARNING
#define LOGW(...)  iLOG((const char *)__func__, (const char *)__LOCATION, LOG_WARNING, __VA_ARGS__)
#else
#define LOGW(...)  ((void)0)
#endif

#if DEBUG_LOG && DEBUG_LOG_LEVEL_NOTICE
#define LOGN(...)  iLOG((const char *)__func__, (const char *)__LOCATION, LOG_NOTICE, __VA_ARGS__)
#else
#define LOGN(...)  ((void)0)
#endif

#if DEBUG_LOG && DEBUG_LOG_LEVEL_INFO
#define LOGI(...)  iLOG((const char *)__func__, (const char *)__LOCATION, LOG_INFO, __VA_ARGS__)
#else
#define LOGI(...)  ((void)0)
#endif

#if DEBUG_LOG && DEBUG_LOG_LEVEL_DEBUG
#define LOGD(...)  iLOG((const char *)__func__, (const char *)__LOCATION, LOG_DEBUG, __VA_ARGS__)
#else
#define LOGD(...)  ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* log.h */
