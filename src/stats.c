/**
    @file stats.c
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
    time_t heartbeat_reset_at; /**< Time when the application was restarted due to late heartbeats (epoch). */
    time_t avg_first_heartbeat_time; /**< Average first heartbeat time (seconds). */
    time_t max_first_heartbeat_time; /**< Maximum first heartbeat time (seconds). */
    time_t min_first_heartbeat_time; /**< Minimum first heartbeat time (seconds). */
    time_t avg_heartbeat_time; /**< Average heartbeat time (seconds). */
    time_t max_heartbeat_time; /**< Maximum heartbeat time (seconds). */
    time_t min_heartbeat_time; /**< Minimum heartbeat time (seconds). */
    size_t start_count; /**< Number of application starts. */
    size_t crash_count; /**< Number of application crashes. */
    size_t heartbeat_count; /**< Number of heartbeats received. */
    size_t heartbeat_count_old; /**< Number of old heartbeats received. */
    size_t heartbeat_reset_count; /**< Number of restarts due to late heartbeats. */
    uint32_t magic; /**< Magic value indicating initialization (STATS_MAGIC when struct is initialized). */
} Statistic_t;

static Statistic_t stats[MAX_APPS]; // statistics for the apps

static void clearHeartbeatCount(int index)
{
    stats[index].heartbeat_count_old = stats[index].heartbeat_count;
    stats[index].heartbeat_count = 0;
}

void stats_started_at(int index)
{
    stats[index].started_at = time(NULL);
    stats[index].start_count++;
    clearHeartbeatCount(index);
}

void stats_crashed_at(int index)
{
    stats[index].crashed_at = time(NULL);
    stats[index].crash_count++;
    clearHeartbeatCount(index);
}

void stats_heartbeat_reset_at(int index)
{
    stats[index].heartbeat_reset_at = time(NULL);
    stats[index].heartbeat_reset_count++;
    clearHeartbeatCount(index);
}

void stats_update_heartbeat_time(int index, time_t heartbeatTime)
{
    stats[index].heartbeat_count++;
    // Calculate average heartbeat time
    stats[index].avg_heartbeat_time = ((stats[index].avg_heartbeat_time * (stats[index].heartbeat_count - 1)) + heartbeatTime) / stats[index].heartbeat_count;

    // Update maximum heartbeat time
    if(heartbeatTime > stats[index].max_heartbeat_time)
    {
        stats[index].max_heartbeat_time = heartbeatTime;
    }

    // Update minimum heartbeat time
    if(heartbeatTime < stats[index].min_heartbeat_time || stats[index].heartbeat_count == 1)
    {
        stats[index].min_heartbeat_time = heartbeatTime;
    }
}

void stats_update_first_heartbeat_time(int index, time_t heartbeatTime)
{
    int start_count = stats[index].start_count + stats[index].crash_count + stats[index].heartbeat_reset_count;
    // Calculate average first heartbeat time
    stats[index].avg_first_heartbeat_time = ((stats[index].avg_first_heartbeat_time * (start_count - 1)) + heartbeatTime) / start_count;

    // Update maximum first heartbeat time
    if(heartbeatTime > stats[index].max_first_heartbeat_time)
    {
        stats[index].max_first_heartbeat_time = heartbeatTime;
    }

    // Update minimum first heartbeat time
    if(heartbeatTime < stats[index].min_first_heartbeat_time || stats[index].start_count == 1)
    {
        stats[index].min_first_heartbeat_time = heartbeatTime;
    }
}

static char *printDate(const time_t *t)
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
    fprintf(fp, "Started at: %s\n", printDate(&stats[index].started_at));
    fprintf(fp, "Crashed at: %s\n", printDate(&stats[index].crashed_at));
    fprintf(fp, "Heartbeat reset at: %s\n", printDate(&stats[index].heartbeat_reset_at));
    fprintf(fp, "Start count: %zu\n", stats[index].start_count);
    fprintf(fp, "Crash count: %zu\n", stats[index].crash_count);
    fprintf(fp, "Heartbeat reset count: %zu\n", stats[index].heartbeat_reset_count);
    fprintf(fp, "Heartbeat count: %zu\n", stats[index].heartbeat_count);
    fprintf(fp, "Heartbeat count old: %zu\n", stats[index].heartbeat_count_old);
    fprintf(fp, "Average first heartbeat time: %lld seconds\n", (long long)stats[index].avg_first_heartbeat_time);
    fprintf(fp, "Maximum first heartbeat time: %lld seconds\n", (long long)stats[index].max_first_heartbeat_time);
    fprintf(fp, "Minimum first heartbeat time: %lld seconds\n", (long long)stats[index].min_first_heartbeat_time);
    fprintf(fp, "Average heartbeat time: %lld seconds\n", (long long)stats[index].avg_heartbeat_time);
    fprintf(fp, "Maximum heartbeat time: %lld seconds\n", (long long)stats[index].max_heartbeat_time);
    fprintf(fp, "Minimum heartbeat time: %lld seconds\n", (long long)stats[index].min_heartbeat_time);
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
