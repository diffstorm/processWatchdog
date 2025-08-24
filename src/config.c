/**
    @file config.c
    @brief Configuration Management Module

    This module handles INI file parsing, validation, and configuration management
    for the Process Watchdog application. It provides functions to load, validate,
    and monitor configuration files.

    @date 2023-01-01
    @version 1.0
    @author by Eray Ozturk | erayozturk1@gmail.com
    @url github.com/diffstorm
    @license GPL-3 License
*/

#include "config.h"
#define INI_MAX_LINE MAX_APP_CMD_LENGTH
#include "ini.h"
#include "log.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

#define MAX_REBOOT_MINUTES 525600 // 1 year in minutes

static time_t config_get_file_modified_time(const char *path)
{
    struct stat attr;
    stat(path, &attr);
    return attr.st_mtime;
}

static int config_ini_handler(void *user, const char *section, const char *name, const char *value)
{
    // Extract apps and state from user data
    Application_t *apps = ((void **)user)[0];
    AppState_t *state = ((void **)user)[1];
    static char last_section[MAX_APP_NAME_LENGTH] = {0};
    const char *app_prefix = "app:";

    // New section detected
    if(strcmp(section, last_section) != 0)
    {
        if(strncmp(section, app_prefix, strlen(app_prefix)) == 0)
        {
            if(state->app_count < MAX_APPS)
            {
                const char *app_name = section + strlen(app_prefix);

                if(*app_name == '\0')
                {
                    LOGE("Empty app name in section header: [%s]", section);
                    return 0; // Error
                }

                strncpy(apps[state->app_count].name, app_name, MAX_APP_NAME_LENGTH - 1);
                apps[state->app_count].name[MAX_APP_NAME_LENGTH - 1] = '\0';
                state->app_count++;
            }
            else
            {
                LOGW("MAX_APPS (%d) reached. Ignoring section [%s]", MAX_APPS, section);
            }
        }

        strncpy(last_section, section, sizeof(last_section) - 1);
        last_section[sizeof(last_section) - 1] = '\0';
    }

    // Find current app index
    int index = -1;

    if(strncmp(section, app_prefix, strlen(app_prefix)) == 0)
    {
        const char *app_name = section + strlen(app_prefix);

        for(int i = 0; i < state->app_count; i++)
        {
            if(strcmp(apps[i].name, app_name) == 0)
            {
                index = i;
                break;
            }
        }
    }

    // Process key-value pairs
    if(strcmp(section, "processWatchdog") == 0)
    {
        if(strcmp(name, "udp_port") == 0)
        {
            if(!parse_int(value, 1, 65535, &state->udp_port))
            {
                LOGE("Invalid UDP port: %s", value);
                return 0;
            }
        }
        else if(strcmp(name, "periodic_reboot") == 0)
        {
            state->periodic_reboot = REBOOT_MODE_DISABLED;
            int hour, min;

            if(sscanf(value, "%d:%d", &hour, &min) == 2)
            {
                if(hour >= 0 && hour <= 23 && min >= 0 && min <= 59)
                {
                    state->periodic_reboot = REBOOT_MODE_DAILY_TIME;
                    state->reboot_params.daily_time.hour = hour;
                    state->reboot_params.daily_time.min = min;
                    LOGN("Periodic reboot set to daily at %02d:%02d", hour, min);
                }
            }
            else
            {
                char unit = 'd';
                long interval = 0;
                char *endptr;
                interval = strtol(value, &endptr, 10);

                if(endptr != value)
                {
                    if(*endptr != '\0')
                    {
                        unit = *endptr;
                    }

                    long multiplier = 0;

                    switch(unit)
                    {
                        case 'h':
                        case 'H':
                            multiplier = 60;
                            break;

                        case 'd':
                        case 'D':
                            multiplier = 24 * 60;
                            break;

                        case 'w':
                        case 'W':
                            multiplier = 7 * 24 * 60;
                            break;

                        case 'm':
                        case 'M':
                            multiplier = 30 * 24 * 60;
                            break;

                        default:
                            break;
                    }

                    if(multiplier > 0 && interval > 0)
                    {
                        if(interval > LONG_MAX / multiplier)
                        {
                            LOGE("Reboot interval value is too large and would cause an overflow.");
                        }
                        else
                        {
                            long interval_minutes = interval * multiplier;

                            if(interval_minutes <= MAX_REBOOT_MINUTES)
                            {
                                state->periodic_reboot = REBOOT_MODE_INTERVAL;
                                state->reboot_params.interval_minutes = interval_minutes;
                                LOGN("Periodic reboot set to an interval of %ld minutes.", interval_minutes);
                            }
                            else
                            {
                                LOGW("Reboot interval of %ld minutes is too long (max is %d minutes).", interval_minutes, MAX_REBOOT_MINUTES);
                            }
                        }
                    }
                }
            }
        }
    }
    else if(index != -1)      // This is an app section we are tracking
    {
        if(strcmp(name, "start_delay") == 0)
        {
            if(!parse_int(value, 0, INT_MAX, &apps[index].start_delay))
            {
                LOGE("Invalid start_delay for app %s: %s", apps[index].name, value);
                return 0;
            }
        }
        else if(strcmp(name, "heartbeat_delay") == 0)
        {
            if(!parse_int(value, 0, INT_MAX, &apps[index].heartbeat_delay))
            {
                LOGE("Invalid heartbeat_delay for app %s: %s", apps[index].name, value);
                return 0;
            }
        }
        else if(strcmp(name, "heartbeat_interval") == 0)
        {
            if(!parse_int(value, 0, INT_MAX, &apps[index].heartbeat_interval))
            {
                LOGE("Invalid heartbeat_interval for app %s: %s", apps[index].name, value);
                return 0;
            }
        }
        else if(strcmp(name, "cmd") == 0)
        {
            snprintf(apps[index].cmd, MAX_APP_CMD_LENGTH, "%s", value);

            if(strlen(value) >= MAX_APP_CMD_LENGTH)
            {
                LOGE("Invalid cmd for app %s - longer than %d characters", apps[index].name, MAX_APP_CMD_LENGTH);
                return 0;
            }
        }
    }

    return 1;
}



int config_validate_file(const char *ini_path)
{
    if(!ini_path || strlen(ini_path) == 0 || strlen(ini_path) >= MAX_APP_CMD_LENGTH)
    {
        return 1;
    }

    if(!f_exist(ini_path))
    {
        return 1;
    }

    return 0;
}

bool config_is_file_updated(const char *ini_path, time_t last_modified)
{
    time_t file_last_modified_time = config_get_file_modified_time(ini_path);
    return (file_last_modified_time != last_modified);
}

int config_parse_file(const char *ini_path, Application_t *apps, int max_apps, AppState_t *state)
{
    // Initialize the application array and state
    memset(apps, 0, sizeof(Application_t) * max_apps);
    state->app_count = 0;
    state->uptime = get_uptime();
    state->udp_port = UDP_PORT;
    state->periodic_reboot = REBOOT_MODE_DISABLED;
    LOGD("Reading ini file %s", ini_path);
    // Prepare user data for the handler
    void *user_data[2] = { apps, state };

    if(ini_parse(ini_path, config_ini_handler, user_data) < 0)
    {
        LOGE("Failed to parse INI file %s", ini_path);
        return 1;
    }

    LOGD("%d processes have found in the ini file %s", state->app_count, ini_path);
    state->ini_last_modified_time = config_get_file_modified_time(ini_path);
    return 0;
}
