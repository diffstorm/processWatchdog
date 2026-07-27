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
#include "apps.h"
#include "heartbeat.h"
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

/**
    @brief Calculates the exponential backoff delay.
    
    @param retry_count Current retry count (1-based).
    @param base_delay Base delay in seconds.
    @return Delay in seconds to wait before next retry.
*/
static int calculate_backoff_delay(int retry_count, int base_delay)
{
    // delay = base_delay * 2^(retry_count - 1)
    // Cap the delay to prevent excessive waits (max 3600 seconds = 1 hour)
    int delay = base_delay * (1 << (retry_count - 1));
    if(delay > 3600)
    {
        delay = 3600;
    }
    return delay;
}

/**
    @brief Checks if the process should be restarted based on backoff policy.
    
    @param app_index Index of the application.
    @return true if restart should proceed, false if waiting for backoff.
*/
static bool should_restart_with_backoff(int app_index)
{
    Application_t *apps = apps_get_array();
    Application_t *app = &apps[app_index];
    
    // If backoff is disabled (max_retries <= 0), always restart
    if(app->max_retries <= 0)
    {
        return true;
    }
    
    // Check if we've exceeded max retries
    if(app->retry_count >= app->max_retries)
    {
        LOGE("Process %s has reached max_retries (%d), stopping retries permanently",
             app->name, app->max_retries);
        return false;
    }
    
    // Check if we need to wait for backoff delay
    if(app->last_failure_time > 0)
    {
        int delay = calculate_backoff_delay(app->retry_count + 1, app->base_delay);
        time_t now = time(NULL);
        time_t elapsed = now - app->last_failure_time;
        
        if(elapsed < delay)
        {
            LOGD("Process %s waiting for backoff: %d/%d seconds remaining",
                 app->name, (int)(delay - elapsed), delay);
            return false;
        }
    }
    
    return true;
}

/**
    @brief Terminates and reaps a child whose identity could not be verified.
*/
static void terminate_unverified_child(pid_t pid, const char *name)
{
    int status;

    if(kill(pid, SIGTERM) < 0 && errno != ESRCH)
    {
        LOGE("Failed to terminate unverified process %s (PID %d): %s",
             name, pid, strerror(errno));
    }

    for(int attempt = 0; attempt < 10; attempt++)
    {
        pid_t result = waitpid(pid, &status, WNOHANG);
        if(result == pid || (result < 0 && errno == ECHILD))
        {
            return;
        }

        if(result < 0 && errno != EINTR)
        {
            LOGE("Failed to wait for unverified process %s (PID %d): %s",
                 name, pid, strerror(errno));
            return;
        }

        usleep(100000);
    }

    LOGE("Unverified process %s (PID %d) did not stop; sending SIGKILL", name, pid);
    if(kill(pid, SIGKILL) < 0 && errno != ESRCH)
    {
        LOGE("Failed to kill unverified process %s (PID %d): %s",
             name, pid, strerror(errno));
    }

    while(waitpid(pid, &status, 0) < 0 && errno == EINTR)
    {
        // Retry interrupted waits so the child is reaped before returning.
    }
}

bool process_is_running(int app_index)
{
    int pid = get_app_pid(app_index);

    if(pid <= 0)
    {
        return false;
    }

    // Check if the application is running
    if(kill(pid, 0) == 0)
    {
        // Verify process identity by checking start time to prevent PID reuse issues
        unsigned long recorded_start_time = apps_get_array()[app_index].start_time;
        unsigned long current_start_time = get_process_start_time(pid);
        
        if(current_start_time == 0 || recorded_start_time == 0)
        {
            LOGE("Unable to verify identity for process %s (PID %d)",
                 get_app_name(app_index), pid);
            return false;
        }

        if(current_start_time != recorded_start_time)
        {
            LOGE("Process %s PID %d has different start time (PID reused), treating as different process",
                 get_app_name(app_index), pid);
            return false;
        }
        
        return true;  // Process is running
    }
    else if(errno == EPERM)
    {
        unsigned long recorded_start_time = apps_get_array()[app_index].start_time;
        unsigned long current_start_time = get_process_start_time(pid);

        if(recorded_start_time > 0 && current_start_time == recorded_start_time)
        {
            LOGE("No permission to signal process %s (PID %d), but its identity is verified",
                 get_app_name(app_index), pid);
            return true;
        }

        LOGE("No permission to verify process %s (PID %d); treating it as not running",
             get_app_name(app_index), pid);
        return false;
    }
    else
    {
        LOGD("Process %s is not running : %s", get_app_name(app_index), strerror(errno));
    }

    return false;
}

bool process_is_started(int app_index)
{
    Application_t *apps = apps_get_array();
    return apps[app_index].started;
}

bool process_is_start_time(int app_index)
{
    Application_t *apps = apps_get_array();
    AppState_t *state = apps_get_state();
    return (get_uptime() - state->uptime) >= (long)apps[app_index].start_delay;
}

void process_start(int app_index)
{
    Application_t *apps = apps_get_array();
    apps[app_index].pid = 0;
    apps[app_index].start_time = 0;
    // Start the application on Linux
    pid_t pid = fork();

    if(pid < 0)
    {
        LOGE("Failed to start process %s, error code: %d - %s", apps[app_index].name, errno, strerror(errno));
    }
    else if(pid == 0)
    {
        // Child process
        // Create new session to detach from parent's terminal/process group
        // The child processes should be started in their own process group so they don't receive signals meant for the watchdog
        setsid();
        // Close inherited file descriptors (except stdin/stdout/stderr)
        int max_fd = (int)sysconf(_SC_OPEN_MAX);

        if(max_fd < 0)
        {
            max_fd = 1024; // Fallback if sysconf fails
        }

        for(int fd = 3; fd < max_fd; fd++)
        {
            close(fd);
        }

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
        // /proc entries can be momentarily unavailable after fork. Retry briefly
        // before treating the child as unverified.
        for(int attempt = 0; attempt < 3 && apps[app_index].start_time == 0; attempt++)
        {
            apps[app_index].start_time = get_process_start_time(pid);
            if(apps[app_index].start_time == 0 && attempt < 2)
            {
                usleep(10000);
            }
        }

        if(apps[app_index].start_time == 0)
        {
            LOGE("Failed to verify identity for process %s (PID %d); terminating it",
                 apps[app_index].name, pid);
            terminate_unverified_child(pid, apps[app_index].name);
            apps[app_index].started = false;
            apps[app_index].first_heartbeat = false;
            apps[app_index].pid = 0;
            return;
        }

        LOGI("Process %s started (PID %d): %s", apps[app_index].name, apps[app_index].pid, apps[app_index].cmd);
        // Use heartbeat module to update heartbeat time
        heartbeat_update_time(app_index);
    }
}

void process_kill(int app_index)
{
    Application_t *apps = apps_get_array();
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
        apps[app_index].start_time = 0;
    }
    else
    {
        LOGE("Failed to terminate process %s", apps[app_index].name);
    }
}

void process_restart(int app_index)
{
    Application_t *apps = apps_get_array();
    Application_t *app = &apps[app_index];
    LOGD("Restarting process %s", app->name);

    // Check if restart should proceed based on backoff policy
    if(!should_restart_with_backoff(app_index))
    {
        LOGD("Process %s restart delayed due to backoff policy", app->name);
        return;
    }

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
        // Increment retry count on failure
        app->retry_count++;
        app->last_failure_time = time(NULL);
        LOGE("Failed to start process %s (retry %d/%d)", app->name, app->retry_count, app->max_retries);
    }
    else
    {
        // Reset retry count on success
        app->retry_count = 0;
        app->last_failure_time = 0;
        // Use heartbeat module to update heartbeat time
        heartbeat_update_time(app_index);
        LOGI("Process %s restarted successfully", app->name);
    }
}
