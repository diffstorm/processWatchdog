/**
    @file stats.h
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

#ifndef STATS_H
#define STATS_H

#include <time.h>

/**
    @file stats.h
    @brief Functions for managing statistics of applications.
*/

// Update statistics functions

/**
    @brief Updates the statistics for when the application started.

    @param index Index of the application.
*/
void stats_started_at(int index);

/**
    @brief Updates the statistics for when the application crashed.

    @param index Index of the application.
*/
void stats_crashed_at(int index);

/**
    @brief Updates the statistics for when the application was restarted due to late heartbeats.

    @param index Index of the application.
*/
void stats_heartbeat_reset_at(int index);

/**
    @brief Updates the statistics for the heartbeat time of the application.

    @param index Index of the application.
    @param heartbeatTime Time in seconds for the heartbeat.
*/
void stats_update_heartbeat_time(int index, time_t heartbeatTime);

/**
    @brief Updates the statistics for the first heartbeat time of the application.

    @param index Index of the application.
    @param heartbeatTime Time in seconds for the first heartbeat.
*/
void stats_update_first_heartbeat_time(int index, time_t heartbeatTime);

// File operations functions

/**
    @brief Prints the statistics to a human-readable file.

    @param index Index of the application.
    @param app_name Name of the application for filename generation.
*/
void stats_print_to_file(int index, const char *app_name);

/**
    @brief Writes the statistics to a raw file for later reading.

    @param index Index of the application.
    @param app_name Name of the application for filename generation.
*/
void stats_write_to_file(int index, const char *app_name);

/**
    @brief Reads the statistics from a raw file to continue from where it left.

    @param index Index of the application.
    @param app_name Name of the application for filename generation.
*/
void stats_read_from_file(int index, const char *app_name);

/**
    @brief Updates CPU usage statistics for the application.

    @param index Index of the application.
    @param pid Process ID of the application.
*/
void stats_update_cpu_usage(int index, int pid);

/**
    @brief Updates memory usage statistics for the application.

    @param index Index of the application.
    @param pid Process ID of the application.
*/
void stats_update_memory_usage(int index, int pid);

#endif // STATS_H
