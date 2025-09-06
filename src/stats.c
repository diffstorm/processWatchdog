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
#include <stdint.h>
#include <unistd.h>

#define STATS_MAGIC ((uint32_t)0xA50FAA55)
#define MAX_FILENAME_LENGTH MAX_APP_NAME_LENGTH

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
    size_t avg_heartbeat_count_old; /**< Average heartbeat count before crashes/resets. */
    // CPU and Memory usage statistics
    float current_cpu_percent; /**< Current CPU usage percentage. */
    float max_cpu_percent; /**< Maximum CPU usage percentage. */
    float min_cpu_percent; /**< Minimum CPU usage percentage. */
    float avg_cpu_percent; /**< Average CPU usage percentage. */
    size_t current_memory_kb; /**< Current memory usage in KB. */
    size_t max_memory_kb; /**< Maximum memory usage in KB. */
    size_t min_memory_kb; /**< Minimum memory usage in KB. */
    size_t avg_memory_kb; /**< Average memory usage in KB. */
    size_t resource_sample_count; /**< Number of resource usage samples taken. */
    uint32_t magic; /**< Magic value indicating initialization (STATS_MAGIC when struct is initialized). */
} Statistic_t;

static Statistic_t stats[MAX_APPS]; // statistics for the apps

typedef struct
{
    unsigned long long prev_process_time;
    struct timespec    prev_ts;
    int initialized;
} CpuState_t;

static CpuState_t cpustates[MAX_APPS] = {0}; // CPU utilization for the apps

/**
    @brief Reads instantaneous CPU usage percentage for a specific process.
          Works regardless of sampling interval. Can exceed 100% on multicore systems.
    @param index Index into cpustates[] array
    @param pid   Process ID
    @return CPU usage percentage, or -1.0 on error
*/
static float get_process_cpu_usage(int index, int pid)
{
    char stat_path[48];
    char buf[512];
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
    FILE *fp = fopen(stat_path, "r");

    if(!fp)
    {
        LOGE("Failed to open %s", stat_path);
        return -1.0f;
    }

    if(!fgets(buf, sizeof(buf), fp))
    {
        fclose(fp);
        LOGE("Failed to read line from %s", stat_path);
        return -1.0f;
    }

    fclose(fp);
    // find end of comm field (inside parentheses)
    char *paren = strrchr(buf, ')');

    if(!paren)
    {
        LOGE("Failed to find closing parenthesis in stat line");
        return -1.0f;
    }

    // tokenize remaining fields after ") "
    char *saveptr;
    char *token = strtok_r(paren + 2, " ", &saveptr);
    int field = 3; // already counted pid(1), comm(2), state(3)
    unsigned long long utime = 0, stime = 0, cutime = 0, cstime = 0;

    while(token)
    {
        field++;

        if(field == 14)
        {
            utime  = strtoull(token, NULL, 10);
        }
        else if(field == 15)
        {
            stime  = strtoull(token, NULL, 10);
        }
        else if(field == 16)
        {
            cutime = strtoull(token, NULL, 10);
        }
        else if(field == 17)
        {
            cstime = strtoull(token, NULL, 10);
            break;
        }

        token = strtok_r(NULL, " ", &saveptr);
    }

    unsigned long long process_time = utime + stime + cutime + cstime;
    LOGD("PID=%d utime=%llu stime=%llu cutime=%llu cstime=%llu total_process_time=%llu",
         pid, utime, stime, cutime, cstime, process_time);
    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    //long nprocs        = sysconf(_SC_NPROCESSORS_ONLN);
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    CpuState_t *st = &cpustates[index];

    /*  if(nprocs <= 0)
        {
        nprocs = 1;
        }*/

    if(st->initialized)
    {
        float elapsed = (float)(now.tv_sec - st->prev_ts.tv_sec) +
                        (float)(now.tv_nsec - st->prev_ts.tv_nsec) / 1e9f;

        if(elapsed < 1e-6f)
        {
            // avoid division by near-zero
            return -1.0f;
        }

        unsigned long long diff = process_time - st->prev_process_time;

        if(process_time < st->prev_process_time)
        {
            // counter wrapped or PID restarted, reset baseline
            st->prev_process_time = process_time;
            st->prev_ts = now;
            LOGD("Process time decreased, resetting baseline for PID=%d", pid);
            return -1.0f;
        }

        float cpu_sec = (float)diff / (float)ticks_per_sec;
        LOGD("PID=%d diff=%llu ticks_per_sec=%ld cpu_sec=%.6f elapsed=%.6f",
             pid, diff, ticks_per_sec, cpu_sec, elapsed);
        st->prev_process_time = process_time;
        st->prev_ts = now;
        float percent = (cpu_sec / elapsed) * 100.0f;
        LOGD("PID=%d CPU usage=%.2f%% (can exceed 100%% on multicore)",
             pid, percent);
        return percent;
    }

    // first call for this pid/index
    st->prev_process_time = process_time;
    st->prev_ts = now;
    st->initialized = 1;
    LOGD("First call for PID=%d, initializing baseline", pid);
    return -1.0f;
}

/**
    @brief Reads memory usage in KB for a specific process from /proc/[pid]/status
    @param pid Process ID
    @return Memory usage in KB, or 0 on error
*/
static size_t get_process_memory_usage(int pid)
{
    char status_path[48];
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);

    if(!f_exist(status_path))
    {
        return 0;
    }

    FILE *fp = fopen(status_path, "r");

    if(!fp)
    {
        return 0;
    }

    char line[256];
    size_t vmrss_kb = 0;

    while(fgets(line, sizeof(line), fp))
    {
        if(strncmp(line, "VmRSS:", 6) == 0)
        {
            sscanf(line, "VmRSS: %zu kB", &vmrss_kb);
            break;
        }
    }

    fclose(fp);
    return vmrss_kb;
}

static void clearHeartbeatCount(int index)
{
    stats[index].heartbeat_count_old = stats[index].heartbeat_count;
    stats[index].heartbeat_count = 0;
}

static void updateHeartbeatCountAverage(int index)
{
    // Update average heartbeat count old when process crashes or gets reset
    if(stats[index].heartbeat_count_old > 0)
    {
        size_t total_events = stats[index].crash_count + stats[index].heartbeat_reset_count;

        if(total_events == 1)
        {
            // First crash/reset
            stats[index].avg_heartbeat_count_old = stats[index].heartbeat_count_old;
        }
        else if(total_events > 1)
        {
            // Update running average
            stats[index].avg_heartbeat_count_old =
                ((stats[index].avg_heartbeat_count_old * (total_events - 1)) +
                 stats[index].heartbeat_count_old) / total_events;
        }
    }
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
    updateHeartbeatCountAverage(index);
    clearHeartbeatCount(index);
}

void stats_heartbeat_reset_at(int index)
{
    stats[index].heartbeat_reset_at = time(NULL);
    stats[index].heartbeat_reset_count++;
    updateHeartbeatCountAverage(index);
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

void stats_update_cpu_usage(int index, int pid)
{
    // Get current CPU usage
    float cpu_percent = get_process_cpu_usage(index, pid);

    if(cpu_percent < 0.0f)
    {
        LOGD("Invalid CPU reading (%.2f%%) for PID=%d, skipping sample", cpu_percent, pid);
        return; // Error reading CPU usage
    }

    // Update current CPU value
    stats[index].current_cpu_percent = cpu_percent;

    // Initialize or update CPU statistics
    if(stats[index].max_cpu_percent == 0.0f && stats[index].min_cpu_percent == 100.0f)
    {
        // First CPU sample or after reset
        stats[index].max_cpu_percent = cpu_percent;
        stats[index].min_cpu_percent = cpu_percent;
        stats[index].avg_cpu_percent = cpu_percent;
    }
    else
    {
        // Update max/min CPU
        if(cpu_percent > stats[index].max_cpu_percent)
        {
            stats[index].max_cpu_percent = cpu_percent;
        }

        if(cpu_percent < stats[index].min_cpu_percent)
        {
            stats[index].min_cpu_percent = cpu_percent;
        }

        // Use exponential moving average for CPU
        float alpha = 0.1f; // Smoothing factor
        stats[index].avg_cpu_percent = stats[index].avg_cpu_percent * (1.0f - alpha) + cpu_percent * alpha;
    }
}

void stats_update_memory_usage(int index, int pid)
{
    // Get current memory usage
    size_t memory_kb = get_process_memory_usage(pid);

    if(memory_kb == 0)
    {
        LOGD("Failed to read memory usage for PID=%d", pid);
        return; // Error reading memory usage
    }

    // Update current memory value
    stats[index].current_memory_kb = memory_kb;
    stats[index].resource_sample_count++;

    // Initialize or update memory statistics
    if(stats[index].resource_sample_count == 1 || stats[index].max_memory_kb == 0)
    {
        // First memory sample
        stats[index].max_memory_kb = memory_kb;
        stats[index].min_memory_kb = memory_kb;
        stats[index].avg_memory_kb = memory_kb;
    }
    else
    {
        // Update max/min memory
        if(memory_kb > stats[index].max_memory_kb)
        {
            stats[index].max_memory_kb = memory_kb;
        }

        if(memory_kb < stats[index].min_memory_kb)
        {
            stats[index].min_memory_kb = memory_kb;
        }

        // Update average memory using running average
        stats[index].avg_memory_kb = ((stats[index].avg_memory_kb * (stats[index].resource_sample_count - 1)) + memory_kb) / stats[index].resource_sample_count;
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

void stats_print_to_file(int index, const char *app_name)
{
    char filename[MAX_FILENAME_LENGTH];
    snprintf(filename, sizeof(filename), "stats_%s.log", app_name);
    FILE *fp = fopen(filename, "w");

    if(fp == NULL)
    {
        LOGE("Error opening file %s", filename);
        return;
    }

    fprintf(fp, "Statistics for App %d %s:\n", index, app_name);
    fprintf(fp, "Started at: %s\n", printDate(&stats[index].started_at));
    fprintf(fp, "Crashed at: %s\n", printDate(&stats[index].crashed_at));
    fprintf(fp, "Heartbeat reset at: %s\n", printDate(&stats[index].heartbeat_reset_at));
    fprintf(fp, "Start count: %zu\n", stats[index].start_count);
    fprintf(fp, "Crash count: %zu\n", stats[index].crash_count);
    fprintf(fp, "Heartbeat reset count: %zu\n", stats[index].heartbeat_reset_count);
    fprintf(fp, "Heartbeat count: %zu\n", stats[index].heartbeat_count);
    fprintf(fp, "Heartbeat count old: %zu\n", stats[index].heartbeat_count_old);
    fprintf(fp, "Average heartbeat count old: %zu\n", stats[index].avg_heartbeat_count_old);
    fprintf(fp, "Average first heartbeat time: %lld seconds\n", (long long)stats[index].avg_first_heartbeat_time);
    fprintf(fp, "Maximum first heartbeat time: %lld seconds\n", (long long)stats[index].max_first_heartbeat_time);
    fprintf(fp, "Minimum first heartbeat time: %lld seconds\n", (long long)stats[index].min_first_heartbeat_time);
    fprintf(fp, "Average heartbeat time: %lld seconds\n", (long long)stats[index].avg_heartbeat_time);
    fprintf(fp, "Maximum heartbeat time: %lld seconds\n", (long long)stats[index].max_heartbeat_time);
    fprintf(fp, "Minimum heartbeat time: %lld seconds\n", (long long)stats[index].min_heartbeat_time);
    fprintf(fp, "Resource sample count: %zu\n", stats[index].resource_sample_count);
    fprintf(fp, "Current CPU usage: %.2f%%\n", stats[index].current_cpu_percent);
    fprintf(fp, "Maximum CPU usage: %.2f%%\n", stats[index].max_cpu_percent);
    fprintf(fp, "Minimum CPU usage: %.2f%%\n", stats[index].min_cpu_percent);
    fprintf(fp, "Average CPU usage: %.2f%%\n", stats[index].avg_cpu_percent);
    char mem_str[32];
    fprintf(fp, "Current memory usage: %s\n", humansize(stats[index].current_memory_kb * 1024, mem_str, sizeof(mem_str)));
    fprintf(fp, "Maximum memory usage: %s\n", humansize(stats[index].max_memory_kb * 1024, mem_str, sizeof(mem_str)));
    fprintf(fp, "Minimum memory usage: %s\n", humansize(stats[index].min_memory_kb * 1024, mem_str, sizeof(mem_str)));
    fprintf(fp, "Average memory usage: %s\n", humansize(stats[index].avg_memory_kb * 1024, mem_str, sizeof(mem_str)));
    fprintf(fp, "Magic: %X\n", stats[index].magic);
    fclose(fp);
    LOGD("Statistics for App %d printed to %s", index, filename);
}

static void resetStatisticsFile(int index, const char *app_name)
{
    if(stats[index].magic != STATS_MAGIC)
    {
        LOGN("Statistics file %s has been reset - magic %X is %X", app_name, stats[index].magic, STATS_MAGIC);
        memset(&stats[index], 0, sizeof(Statistic_t));
        stats[index].magic = STATS_MAGIC;
    }
}

void stats_write_to_file(int index, const char *app_name)
{
    char filename[MAX_FILENAME_LENGTH];
    resetStatisticsFile(index, app_name);
    snprintf(filename, sizeof(filename), "stats_%s.raw", app_name);
    f_write(filename, (char *)&stats[index], sizeof(Statistic_t));
}

void stats_read_from_file(int index, const char *app_name)
{
    char filename[MAX_FILENAME_LENGTH];
    snprintf(filename, sizeof(filename), "stats_%s.raw", app_name);

    if(!f_exist(filename))
    {
        stats_write_to_file(index, app_name);
    }
    else
    {
        f_read(filename, (char *)&stats[index], sizeof(Statistic_t));
        resetStatisticsFile(index, app_name);
    }
}
