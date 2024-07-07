/**
    @file stats.c
    @brief Process Watchdog Application Manager

    The Process Watchdog application manages the processes listed in the configuration file.
    It listens to a specified UDP port for heartbeat messages from these processes, which must
    periodically send their PID. If any process stops running or fails to send its PID over UDP
    within the expected interval, the Process Watchdog application will restart the process.

    The application ensures high reliability and availability by continuously monitoring and
    restarting processes as necessary. It also logs various statistics about the monitored
    processes, including start times, crash times, and ping intervals.

    @date 2023-01-01
    @version 1.0
    @author by Eray Ozturk | erayozturk1@gmail.com
    @url github.com/diffstorm
    @license GPL-3 License
*/

#include "apps.h"
#include "log.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define STATS_MAGIC ((uint32_t)0xA50FAA55)

/**
    @brief Structure that holds data for each application.
*/
typedef struct
{
    time_t started_at; /**< Time when the application started (epoch). */
    time_t crashed_at; /**< Time when the application crashed (epoch). */
    time_t ping_reset_at; /**< Time when the application was restarted due to late pings (epoch). */
    time_t avg_first_ping_time; /**< Average first ping time (seconds). */
    time_t max_first_ping_time; /**< Maximum first ping time (seconds). */
    time_t min_first_ping_time; /**< Minimum first ping time (seconds). */
    time_t avg_ping_time; /**< Average ping time (seconds). */
    time_t max_ping_time; /**< Maximum ping time (seconds). */
    time_t min_ping_time; /**< Minimum ping time (seconds). */
    size_t start_count; /**< Number of application starts. */
    size_t crash_count; /**< Number of application crashes. */
    size_t ping_count; /**< Number of pings received. */
    size_t ping_count_old; /**< Number of old pings received. */
    size_t ping_reset_count; /**< Number of restarts due to late pings. */
    uint32_t magic; /**< Magic value indicating initialization (STATS_MAGIC when struct is initialized). */
} Statistic_t;

static Statistic_t stats[MAX_APPS]; // statistics for the apps

static void clearPingCount(int index)
{
    stats[index].ping_count_old = stats[index].ping_count;
    stats[index].ping_count = 0;
}

void stats_started_at(int index)
{
    stats[index].started_at = time(NULL);
    stats[index].start_count++;
    clearPingCount(index);
}

void stats_crashed_at(int index)
{
    stats[index].crashed_at = time(NULL);
    stats[index].crash_count++;
    clearPingCount(index);
}

void stats_ping_reset_at(int index)
{
    stats[index].ping_reset_at = time(NULL);
    stats[index].ping_reset_count++;
    clearPingCount(index);
}

void stats_update_ping_time(int index, time_t pingTime)
{
    stats[index].ping_count++;
    // Calculate average ping time
    stats[index].avg_ping_time = ((stats[index].avg_ping_time * (stats[index].ping_count - 1)) + pingTime) / stats[index].ping_count;

    // Update maximum ping time
    if(pingTime > stats[index].max_ping_time)
    {
        stats[index].max_ping_time = pingTime;
    }

    // Update minimum ping time
    if(pingTime < stats[index].min_ping_time || stats[index].ping_count == 1)
    {
        stats[index].min_ping_time = pingTime;
    }
}

void stats_update_first_ping_time(int index, time_t pingTime)
{
    int start_count = stats[index].start_count + stats[index].crash_count + stats[index].ping_reset_count;
    // Calculate average first ping time
    stats[index].avg_first_ping_time = ((stats[index].avg_first_ping_time * (start_count - 1)) + pingTime) / start_count;

    // Update maximum first ping time
    if(pingTime > stats[index].max_first_ping_time)
    {
        stats[index].max_first_ping_time = pingTime;
    }

    // Update minimum first ping time
    if(pingTime < stats[index].min_first_ping_time || stats[index].start_count == 1)
    {
        stats[index].min_first_ping_time = pingTime;
    }
}

static char *print_date(const time_t *t)
{
    static char ts[22]; // Fixme : solve without internal static buffer

    if((*t) > 0)
    {
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(t));
    }
    else
    {
        strcpy(ts, "Never");
    }

    return ts;
}

void stats_print_to_file(int index)
{
    char filename[MAX_APP_NAME_LENGTH * 2];
    sprintf(filename, "stats_%s.log", get_app_name(index));
    FILE *fp = fopen(filename, "w");

    if(fp == NULL)
    {
        LOGE("Error opening file %s", filename);
        return;
    }

    fprintf(fp, "Statistics for App %d %s:\n", index, get_app_name(index));
    fprintf(fp, "Started at: %s\n", print_date(&stats[index].started_at));
    fprintf(fp, "Crashed at: %s\n", print_date(&stats[index].crashed_at));
    fprintf(fp, "Ping reset at: %s\n", print_date(&stats[index].ping_reset_at));
    fprintf(fp, "Start count: %zu\n", stats[index].start_count);
    fprintf(fp, "Crash count: %zu\n", stats[index].crash_count);
    fprintf(fp, "Ping reset count: %zu\n", stats[index].ping_reset_count);
    fprintf(fp, "Ping count: %zu\n", stats[index].ping_count);
    fprintf(fp, "Ping count old: %zu\n", stats[index].ping_count_old);
    fprintf(fp, "Average first ping time: %lld seconds\n", (long long)stats[index].avg_first_ping_time);
    fprintf(fp, "Maximum first ping time: %lld seconds\n", (long long)stats[index].max_first_ping_time);
    fprintf(fp, "Minimum first ping time: %lld seconds\n", (long long)stats[index].min_first_ping_time);
    fprintf(fp, "Average ping time: %lld seconds\n", (long long)stats[index].avg_ping_time);
    fprintf(fp, "Maximum ping time: %lld seconds\n", (long long)stats[index].max_ping_time);
    fprintf(fp, "Minimum ping time: %lld seconds\n", (long long)stats[index].min_ping_time);
    fprintf(fp, "Magic: %X\n", stats[index].magic);
    fclose(fp);
    LOGD("Statistics for App %d printed to %s", index, filename);
}

static void resetStatisticsFile(int index)
{
    if(stats[index].magic != STATS_MAGIC)
    {
        LOGN("Statistic file %s has been reset - magic %X is %X", get_app_name(index), stats[index].magic, STATS_MAGIC);
        memset(&stats[index], 0, sizeof(Statistic_t));
        stats[index].magic = STATS_MAGIC;
    }
}

void stats_write_to_file(int index)
{
    char filename[MAX_APP_NAME_LENGTH * 2];
    resetStatisticsFile(index);
    sprintf(filename, "stats_%s.raw", get_app_name(index));
    f_write(filename, (char *)&stats[index], sizeof(Statistic_t));
}

void stats_read_from_file(int index)
{
    char filename[MAX_APP_NAME_LENGTH * 2];
    sprintf(filename, "stats_%s.raw", get_app_name(index));

    if(!f_exist(filename))
    {
        stats_write_to_file(index);
    }
    else
    {
        f_read(filename, (char *)&stats[index], sizeof(Statistic_t));
        resetStatisticsFile(index);
    }
}
