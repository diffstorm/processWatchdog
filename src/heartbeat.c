/**
    @file heartbeat.c
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

#include "heartbeat.h"
#include "apps.h"
#include "log.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

void heartbeat_update_time(int app_index)
{
    Application_t *apps = apps_get_array();
    apps[app_index].last_heartbeat = time(NULL);
    LOGD("Heartbeat time updated for %s", apps[app_index].name);
}

time_t heartbeat_get_elapsed_time(int app_index)
{
    Application_t *apps = apps_get_array();
    time_t now = time(NULL);
    return now - apps[app_index].last_heartbeat;
}

bool heartbeat_is_timeout(int app_index)
{
    Application_t *apps = apps_get_array();
    const Application_t *app = &apps[app_index];

    if(!app->started)
    {
        return false;    // App not running yet
    }

    if(app->heartbeat_interval <= 0)
    {
        return false;    // Heartbeat not expected for this app
    }

    const time_t now = time(NULL);

    if(now < app->last_heartbeat)
    {
        LOGW("Time anomaly detected for %s (system clock changed?)", app->name);
        heartbeat_update_time(app_index);
        return false;  // Reset and give another interval
    }

    const time_t first_heartbeat_threshold = (time_t)MAX(app->heartbeat_interval, app->heartbeat_delay); // delay is designed to be larger than interval
    const time_t regular_threshold = (time_t)app->heartbeat_interval;
    const time_t threshold = app->first_heartbeat ? regular_threshold : first_heartbeat_threshold;
    const time_t elapsed = now - app->last_heartbeat;

    if(elapsed >= threshold)
    {
        LOGD("Heartbeat time up for %s", app->name);
        return true;
    }

    return false;
}

void heartbeat_set_first_received(int app_index)
{
    Application_t *apps = apps_get_array();
    apps[app_index].first_heartbeat = true;
}

bool heartbeat_get_first_received(int app_index)
{
    Application_t *apps = apps_get_array();
    return apps[app_index].first_heartbeat;
}
