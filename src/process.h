/**
    @file process.h
    @brief Process Lifecycle Management Module

    This module handles process lifecycle operations including starting, stopping,
    restarting, and monitoring processes for the Process Watchdog application.
    It provides functions to manage individual application processes.

    @date 2023-01-01
    @version 1.0
    @author by Eray Ozturk | erayozturk1@gmail.com
    @url github.com/diffstorm
    @license GPL-3 License
*/

#ifndef PROCESS_H
#define PROCESS_H

#include <stdbool.h>

/**
    @brief Checks if the specified application is currently running.

    @param app_index Index of the application.
    @return true if the application is running, false otherwise.
*/
bool process_is_running(int app_index);

/**
    @brief Checks if the specified application has been started.

    @param app_index Index of the application.
    @return true if the application has been started, false otherwise.
*/
bool process_is_started(int app_index);

/**
    @brief Checks if it is time to start the specified application based on the start delay.

    @param app_index Index of the application.
    @return true if it is time to start the application, false otherwise.
*/
bool process_is_start_time(int app_index);

/**
    @brief Starts the specified application.

    @param app_index Index of the application.
*/
void process_start(int app_index);

/**
    @brief Stops the specified application by killing its process.

    @param app_index Index of the application.
*/
void process_kill(int app_index);

/**
    @brief Restarts the specified application.

    @param app_index Index of the application.
*/
void process_restart(int app_index);

/**
    @brief Resumes an app after a manual cooldown acknowledgment.

    Restarts the app and clears its cooldown / circuit-breaker state. Shared by the
    resume<app> file command and the k<app> UDP command.

    @param app_index Index of the application.
*/
void process_resume(int app_index);

/**
    @brief Computes the exponential-backoff delay in seconds (clamped to 1 hour).

    Exposed for unit testing; used by the restart policy.

    @param retry_count Current retry count (1-based).
    @param base_delay Base delay in seconds.
    @return Delay in seconds, in [0, 3600].
*/
int calculate_backoff_delay(int retry_count, int base_delay);

#endif // PROCESS_H