/**
    @file heartbeat.h
    @brief Heartbeat Management Module

    This module handles heartbeat timing logic including timeout calculations,
    timestamp updates, and first heartbeat tracking for the Process Watchdog
    application. It provides functions to manage heartbeat timing for individual
    application processes.

    @date 2023-01-01
    @version 1.0
    @author by Eray Ozturk | erayozturk1@gmail.com
    @url github.com/diffstorm
    @license GPL-3 License
*/

#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <stdbool.h>
#include <time.h>

/**
    @brief Updates the last heartbeat time for the specified application.

    @param app_index Index of the application.
*/
void heartbeat_update_time(int app_index);

/**
    @brief Gets the elapsed time since the last heartbeat received from the specified application.

    @param app_index Index of the application.
    @return Elapsed time in seconds.
*/
time_t heartbeat_get_elapsed_time(int app_index);

/**
    @brief Checks if it is time to expect a heartbeat from the specified application.

    @param app_index Index of the application.
    @return true if it is time to expect a heartbeat, false otherwise.
*/
bool heartbeat_is_timeout(int app_index);

/**
    @brief Sets the flag indicating that the specified application has sent its first heartbeat.

    @param app_index Index of the application.
*/
void heartbeat_set_first_received(int app_index);

/**
    @brief Gets the flag indicating whether the specified application has sent its first heartbeat.

    @param app_index Index of the application.
    @return true if the application has sent its first heartbeat, false otherwise.
*/
bool heartbeat_get_first_received(int app_index);

#endif // HEARTBEAT_H