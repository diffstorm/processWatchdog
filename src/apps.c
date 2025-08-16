/**
    @file apps.c
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
#include "config.h"
#include "process.h"
#include "log.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>



static Application_t apps[MAX_APPS]; /**< Array of Application_t structures representing applications defined in the ini file. */
static AppState_t app_state = {0};

//------------------------------------------------------------------

void print_app(int i)
{
    LOGN("## Application info [%d]", i);
    LOGN("%d- name              : %s", i, apps[i].name);
    LOGN("%d- start_delay       : %d", i, apps[i].start_delay);
    LOGN("%d- heartbeat_delay   : %d", i, apps[i].heartbeat_delay);
    LOGN("%d- heartbeat_interval: %d", i, apps[i].heartbeat_interval);
    LOGN("%d- cmd               : %s", i, apps[i].cmd);
    LOGN("%d- started           : %d", i, apps[i].started);
    LOGN("%d- first_heartbeat   : %d", i, apps[i].first_heartbeat);
    LOGN("%d- pid               : %d", i, apps[i].pid);
    LOGN("%d- last_heartbeat    : %d", i, apps[i].last_heartbeat);
}

//------------------------------------------------------------------

void update_heartbeat_time(int i)
{
    apps[i].last_heartbeat = time(NULL);
    LOGD("Heartbeat time updated for %s", apps[i].name);
}

int find_pid(int pid)
{
    for(int i = 0; i < app_state.app_count; i++)
    {
        if(0 < apps[i].pid && pid == apps[i].pid)
        {
            return i;
        }
    }

    return -1;
}

time_t get_heartbeat_time(int i)
{
    time_t now = time(NULL);
    return now - apps[i].last_heartbeat;
}

bool is_timeup(int i)
{
    const Application_t *app = &apps[i];

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
        update_heartbeat_time(i);
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

void set_first_heartbeat(int i)
{
    apps[i].first_heartbeat = true;
}

bool get_first_heartbeat(int i)
{
    return apps[i].first_heartbeat;
}

//------------------------------------------------------------------

int read_ini_file()
{
    // Use default ini file if not already set
    if(config_validate_file(app_state.ini_file))
    {
        LOGD("Using default ini file %s", INI_FILE);
        snprintf(app_state.ini_file, MAX_APP_CMD_LENGTH, "%s", INI_FILE);
    }

    // Use the config module to parse the file
    return config_parse_file(app_state.ini_file, apps, MAX_APPS, &app_state);
}

bool is_ini_updated()
{
    return config_is_file_updated(app_state.ini_file, app_state.ini_last_modified_time);
}

int set_ini_file(char *path)
{
    if(config_validate_file(path))
    {
        LOGE("Invalid path");
        return 1;
    }

    snprintf(app_state.ini_file, MAX_APP_CMD_LENGTH, "%s", path);
    LOGD("INI file set to: %s", app_state.ini_file);
    return 0;
}

//------------------------------------------------------------------

// Wrapper functions for backward compatibility - delegate to process module
// TODO : inline wrapper functions
bool is_application_running(int i)
{
    return process_is_running(i);
}

bool is_application_started(int i)
{
    return process_is_started(i);
}

bool is_application_start_time(int i)
{
    return process_is_start_time(i);
}

void start_application(int i)
{
    process_start(i);
}

void kill_application(int i)
{
    process_kill(i);
}

void restart_application(int i)
{
    process_restart(i);
}

int get_app_count(void)
{
    return app_state.app_count;
}

char *get_app_name(int i)
{
    return apps[i].name;
}

int get_udp_port(void)
{
    return app_state.udp_port;
}

//------------------------------------------------------------------
// External access functions for other modules
//------------------------------------------------------------------

Application_t* apps_get_array(void)
{
    return apps;
}

AppState_t* apps_get_state(void)
{
    return &app_state;
}
