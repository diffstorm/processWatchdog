/**
    @file log.c
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

#include "log.h"
#include "utils.h"

#include <stdarg.h>
#include <syslog.h>
#include <pthread.h>

const char *log_levels[] =
{
    "Emergency",
    "Alert",
    "Critical",
    "Error",
    "Warning",
    "Notice",
    "Info",
    "Debug",
    NULL
};

static pthread_mutex_t syslog_lock = PTHREAD_MUTEX_INITIALIZER; // mutex

void syslog_mutex_lock()
{
    pthread_mutex_lock(&syslog_lock);
}

void syslog_mutex_unlock()
{
    pthread_mutex_unlock(&syslog_lock);
}

#if DEBUG_LOG && (DEBUG_LOG_LEVEL_ERROR || DEBUG_LOG_LEVEL_WARNING || DEBUG_LOG_LEVEL_INFO || DEBUG_LOG_LEVEL_DEBUG)

#if DEBUG_LOG_TABLE_VIEW
// Info    <file_name>:<line>   <function_name>   Test message
#define _LOG_FORMAT "[%s] %-10s %-20.20s %-24.24s %s\r\n"
#define _LOG_VARS   timestamp(ts, sizeof(ts)), log_levels[type], loc, function, buffer

#define _LOG_FORMAT_SHORT "[%s] %-10s %-20.20s %-24.24s\r\n"
#define _LOG_VARS_SHORT   timestamp(ts, sizeof(ts)), log_levels[type], loc, function

void iLOG(const char *function, const char *location, log_priority_t type, const char *format, ...)
{
    char buffer[2 * 2048];
    char out[2 * 2048];
    char loc[128];
    char ts[22];
    int len;
    syslog_mutex_lock();
    // translate into buffer
    {
        va_list args;
        va_start(args, format);
        len = vsnprintf(buffer, sizeof(buffer) - 1, format, args);
        va_end(args);
    }
#if SYSLOG_LOG
    // log into local
    {
        setlogmask(LOG_UPTO(SYSLOG_LOG_LEVEL));
        openlog("wdt", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);
        //syslog (LOG_MAKEPRI(LOG_LOCAL0, (int)type), "%s", buffer);
        syslog((int)type, "%s", buffer);
        closelog();
    }
#endif
    // discard src/ part
    char *p = strchr(location, '/');

    if(NULL != p)
    {
        pstrcpy(loc, p + 1);
    }
    else
    {
        pstrcpy(loc, location);
    }

    if(0 < len)
    {
        snprintf(out, sizeof(out), _LOG_FORMAT, _LOG_VARS);
    }
    else
    {
        snprintf(out, sizeof(out), _LOG_FORMAT_SHORT, _LOG_VARS_SHORT);
    }

    FILE *fp = stdout;

    switch(type)
    {
        case LOG_EMERG:
        case LOG_ALERT:
        case LOG_CRIT:
        case LOG_ERR:
            fp = stderr;
            fprintf(fp, RED "%s" RESET, out);
            break;

        case LOG_WARNING:
            fprintf(fp, YELLOW "%s" RESET, out);
            break;

        case LOG_INFO:
            fprintf(fp, BLUE "%s" RESET, out);
            break;

        default:
            fprintf(fp, "%s", out);
            break;
    }

    fflush(fp);
#if DEBUG_LOG_LEVEL_FILE

    if(type <= FILE_LOG_LEVEL)
    {
        static int file_check = 100;

        if(--file_check <= 0)
        {
            file_check = 100;

            if(f_exist(DEBUG_LOG_FILENAME))
            {
                if(f_size(DEBUG_LOG_FILENAME) > FILE_LOG_SIZE_MAX)
                {
                    f_rename(DEBUG_LOG_FILENAME, DEBUG_LOG_OLD_FILENAME);
                }
            }
        }

        fp = fopen(DEBUG_LOG_FILENAME, "a");

        if(fp != NULL)
        {
            fprintf(fp, "%s", out);
            fclose(fp);
        }
    }

#endif
    syslog_mutex_unlock();
}

#else

// Info : Test message | <function_name> @ <file_name> : <line>
void iLOG(const char *function, const char *location, log_priority_t type, const char *format, ...)
{
    unsigned int length = 0;
    const char separator[] = ": ";
    const char pipe[] = " | ";
    const char newline[] = "\r\n";
    char *out;
    char *p;
    char buffer[4096];
    int len;
    syslog_mutex_lock();
    // translate into buffer
    {
        va_list args;
        va_start(args, format);
        len = vsnprintf(buffer, sizeof(buffer) - 1, format, args);
        va_end(args);
    }
#if SYSLOG_LOG
    // log into local
    {
        setlogmask(LOG_UPTO(SYSLOG_LOG_LEVEL));
        openlog("wdt", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);
        //syslog (LOG_MAKEPRI(LOG_LOCAL0, (int)type), "%s", buffer);
        syslog((int)type, "%s", buffer);
        closelog();
    }
#endif
    length += strlen(log_levels[type]);
    length += sizeof(separator);

    if(0 < len)
    {
        length += sizeof(pipe);
    }

    length += len;
    length += strlen(function);
    length += strlen(location);
    length += sizeof(newline);
    out = (char *)malloc(length);

    if(NULL != out)
    {
        p = out;
        p = pstrcpy(p, log_levels[type]);
        p = pstrcpy(p, separator);
        p = pstrcpy(p, buffer);

        if(0 < len)
        {
            p = pstrcpy(p, pipe);
        }

        p = pstrcpy(p, function);
        p = pstrcpy(p, location);
        p = pstrcpy(p, newline);
        FILE *fp = stdout;

        switch(type)
        {
            case LOG_EMERG:
            case LOG_ALERT:
            case LOG_CRIT:
            case LOG_ERR:
                fp = stderr;
                break;

            default:
                break;
        }

        fprintf(fp, "%s", out);
        fflush(fp);
#if DEBUG_LOG_LEVEL_FILE

        if(type <= FILE_LOG_LEVEL)
        {
            static int file_check = 100;

            if(--file_check <= 0)
            {
                file_check = 100;

                if(f_exist(DEBUG_LOG_FILENAME))
                {
                    if(f_size(DEBUG_LOG_FILENAME) > FILE_LOG_SIZE_MAX)
                    {
                        f_rename(DEBUG_LOG_FILENAME, DEBUG_LOG_OLD_FILENAME);
                    }
                }
            }

            fp = fopen(DEBUG_LOG_FILENAME, "a");

            if(fp != NULL)
            {
                fprintf(fp, "%s", out);
                fclose(fp);
            }
        }

#endif
        free(out);
    }

    syslog_mutex_unlock();
}
#endif // DEBUG_LOG_TABLE_VIEW

#endif
