/**
    @file process.c
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

#include "process.h"
#include "apps.h"  // For Application_t and access to application data
#include "log.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

//------------------------------------------------------------------
// External access to application data (from apps.c)
//------------------------------------------------------------------

// These functions will be implemented in apps.c to provide access to application data
extern Application_t* apps_get_array(void);
extern AppState_t* apps_get_state(void);

//------------------------------------------------------------------
// Public interface functions
//------------------------------------------------------------------

bool process_is_running(int app_index)
{
    Application_t* apps = apps_get_array();
    
    if(apps[app_index].pid <= 0)
    {
        return false;
    }

    // Check if the application is running
    if(kill(apps[app_index].pid, 0) == 0)
    {
        return true;  // Process is running
    }
    else if(errno == EPERM)
    {
        LOGE("No permission to check if process %s is running : %s", apps[app_index].name, strerror(errno));
        return true;
    }
    else
    {
        LOGD("Process %s is not running : %s", apps[app_index].name, strerror(errno));
    }

    return false;
}

bool process_is_started(int app_index)
{
    Application_t* apps = apps_get_array();
    return apps[app_index].started;
}

bool process_is_start_time(int app_index)
{
    Application_t* apps = apps_get_array();
    AppState_t* state = apps_get_state();
    return (get_uptime() - state->uptime) >= (long)apps[app_index].start_delay;
}

void process_start(int app_index)
{
    Application_t* apps = apps_get_array();
    
    apps[app_index].pid = 0;
    // Start the application on Linux
    pid_t pid = fork();

    if(pid < 0)
    {
        LOGE("Failed to start process %s, error code: %d - %s", apps[app_index].name, errno, strerror(errno));
    }
    else if(pid == 0)
    {
        // Child process
        // Reset signals to default
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);
        sigaction(SIGUSR1, &sa, NULL);
        sigaction(SIGUSR2, &sa, NULL);
        LOGD("Starting the process %s with CMD : %s", apps[app_index].name, apps[app_index].cmd);
        run_command(apps[app_index].cmd);
        LOGE("Process %s stopped running", apps[app_index].name);
        exit(EXIT_FAILURE); // exit child process
    }
    else
    {
        // Parent process
        apps[app_index].started = true;
        apps[app_index].first_heartbeat = false;
        apps[app_index].pid = pid;
        LOGI("Process %s started (PID %d): %s", apps[app_index].name, apps[app_index].pid, apps[app_index].cmd);
        // Note: update_heartbeat_time will be handled by heartbeat module in Phase 3
        // For now, we'll call it directly (it's still in apps.c)
        extern void update_heartbeat_time(int i);
        update_heartbeat_time(app_index);
    }
}

void process_kill(int app_index)
{
    Application_t* apps = apps_get_array();
    bool killed = false;
    LOGD("Killing process %s", apps[app_index].name);

    if(apps[app_index].pid <= 0)
    {
        return;
    }

    if(kill(apps[app_index].pid, SIGTERM) < 0 && errno != ESRCH)
    {
        LOGE("Failed to terminate process %s, error: %d - %s", apps[app_index].name, errno, strerror(errno));
    }

    int status;
    int max_wait = MAX_WAIT_PROCESS_TERMINATION; // [seconds]
    LOGD("Waiting for the process %s", apps[app_index].name);

    do
    {
        sleep(1);
        int ret = waitpid(apps[app_index].pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

        if(ret == 0)
        {
            LOGD("Process %s is still running", apps[app_index].name);
        }
        else if(ret < 0)
        {
            if(errno == ECHILD)
            {
                LOGD("Process %s already terminated", apps[app_index].name);
                max_wait = 0;
            }
            else
            {
                LOGE("Failed to wait for process %s, error: %d - %s", apps[app_index].name, errno, strerror(errno));
            }
        }
        else if(ret > 0)
        {
            if(WIFEXITED(status))
            {
                LOGD("Process %s exited, status=%d", apps[app_index].name, WEXITSTATUS(status));
                max_wait = 0;
            }
            else if(WIFSIGNALED(status))
            {
                LOGD("Process %s killed by signal %d", apps[app_index].name, WTERMSIG(status));
                max_wait = 0;
            }
            else if(WIFSTOPPED(status))
            {
                LOGD("Process %s stopped by signal %d", apps[app_index].name, WSTOPSIG(status));
                max_wait = 0;
            }
        }

        max_wait--;
    }
    while(max_wait > 0);

    if(process_is_running(app_index))
    {
        LOGD("Sending SIGKILL to process %s", apps[app_index].name);

        if(kill(apps[app_index].pid, SIGKILL) < 0 && errno != ESRCH)
        {
            LOGE("Failed to kill process %s, error: %d - %s", apps[app_index].name, errno, strerror(errno));
        }
        else
        {
            LOGI("Process %s killed", apps[app_index].name);

            if(!process_is_running(app_index))
            {
                killed = true;
            }
        }
    }
    else
    {
        LOGI("Process %s terminated", apps[app_index].name);
        killed = true;
    }

    if(killed)
    {
        apps[app_index].started = false;
        apps[app_index].first_heartbeat = false;
        apps[app_index].pid = 0;
    }
    else
    {
        LOGE("Failed to terminate process %s", apps[app_index].name);
    }
}

void process_restart(int app_index)
{
    Application_t* apps = apps_get_array();
    LOGD("Restarting process %s", apps[app_index].name);

    if(process_is_running(app_index))
    {
        process_kill(app_index);
    }

    process_start(app_index);
    // Wait for the application to start
    int wait_time = 0;

    while(wait_time < MAX_WAIT_PROCESS_START)
    {
        sleep(1);

        if(process_is_running(app_index))
        {
            break;
        }

        wait_time++;
    }

    if(!process_is_running(app_index))
    {
        LOGE("Failed to start process %s", apps[app_index].name);
    }
    else
    {
        // Note: update_heartbeat_time will be handled by heartbeat module in Phase 3
        // For now, we'll call it directly (it's still in apps.c)
        extern void update_heartbeat_time(int i);
        update_heartbeat_time(app_index);
        LOGI("Process %s restarted successfully", apps[app_index].name);
    }
}