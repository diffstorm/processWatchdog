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
#define INI_MAX_LINE MAX_APP_CMD_LENGTH
#include "ini.h"
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

/**
    @struct Application_t
    @brief Structure representing an application defined in the ini file.
*/
typedef struct
{
    // In the ini file
    int start_delay; /**< Delay in seconds before starting the application. */
    int heartbeat_delay; /**< Time in seconds to wait before expecting a heartbeat from the application. */
    int heartbeat_interval; /**< Maximum time period in seconds between heartbeats. */
    char name[MAX_APP_NAME_LENGTH]; /**< Name of the application. */
    char cmd[MAX_APP_CMD_LENGTH]; /**< Command to start the application. */
    // Not in the ini file
    bool started; /**< Flag indicating whether the application has been started. */
    bool first_heartbeat; /**< Flag indicating whether the application has sent its first heartbeat. */
    int pid; /**< Process ID of the application. */
    time_t last_heartbeat; /**< Time when the last heartbeat was received from the application. */
} Application_t;

typedef struct
{
    int app_count; /**< Total number of applications found in the ini file. */
    int udp_port; /**< UDP port number specified in the ini file. */
    char ini_file[MAX_APP_CMD_LENGTH]; /**< Path to the ini file. */
    time_t ini_last_modified_time; /**< Last modified time of the ini file. */
    time_t uptime; /**< System uptime in seconds. */
    int ini_index; /**< Index used to read an array in the ini file. */
} AppState_t;

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
    time_t t = time(NULL);
    return t - apps[i].last_heartbeat;
}

bool is_timeup(int i)
{
    bool ret = false;
    time_t t = time(NULL);

    if(t < apps[i].last_heartbeat)
    {
        update_heartbeat_time(i);
    }

    if(t - apps[i].last_heartbeat >= (apps[i].first_heartbeat ? apps[i].heartbeat_interval : apps[i].heartbeat_delay))
    {
        ret = true;
        LOGD("Heartbeat time up for %s", apps[i].name);
    }

    return ret;
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

static time_t file_modified_time(char *path)
{
    struct stat attr;
    stat(path, &attr);
    return attr.st_mtime;
}

bool is_ini_updated()
{
    time_t file_last_modified_time = file_modified_time(app_state.ini_file);
    return (file_last_modified_time != app_state.ini_last_modified_time);
}

static int check_ini_file(char *path)
{
    if(!path || strlen(path) == 0 || strlen(path) >= MAX_APP_CMD_LENGTH)
    {
        return 1;
    }

    if(!f_exist(path))
    {
        return 1;
    }

    return 0;
}

int set_ini_file(char *path)
{
    if(check_ini_file(path))
    {
        LOGE("Invalid path");
        return 1;
    }

    snprintf(app_state.ini_file, MAX_APP_CMD_LENGTH, "%s", path);
    LOGD("INI file set to: %s", app_state.ini_file);
    return 0;
}

static int handler(void *user, const char *section, const char *name, const char *value)
{
    (void)(user);
    const char *target_section = "processWatchdog";
    char expected_name[INI_MAX_LINE];

    if(strcmp(section, target_section) != 0)
    {
        return 1;
    }

    // Global parameters
    if(strcmp(name, "udp_port") == 0)
    {
        if(!parse_int(value, 1, 65535, &app_state.udp_port))
        {
            LOGE("Invalid UDP port: %s", value);
            return 0;
        }

        return 1;
    }

    if(strcmp(name, "n_apps") == 0)
    {
        if(!parse_int(value, 0, MAX_APPS, &app_state.app_count))
        {
            LOGE("Invalid n_apps: %s", value);
            return 0;
        }

        return 1;
    }

    // Application parameters
    int index = app_state.ini_index;

    if(index >= app_state.app_count)
    {
        return 1;
    }

#define GEN_NAME(field) snprintf(expected_name, sizeof(expected_name), "%d_%s", index + 1, field)

    if(GEN_NAME("name"), strcmp(name, expected_name) == 0)
    {
        snprintf(apps[index].name, MAX_APP_NAME_LENGTH, "%s", value);

        if(strlen(value) >= MAX_APP_NAME_LENGTH)
        {
            LOGW("App %d name truncated", index);
        }
    }
    else if(GEN_NAME("start_delay"), strcmp(name, expected_name) == 0)
    {
        if(!parse_int(value, 0, INT_MAX, &apps[index].start_delay))
        {
            LOGE("Invalid start_delay for app %d: %s", index, value);
            return 0;
        }
    }
    else if(GEN_NAME("heartbeat_delay"), strcmp(name, expected_name) == 0)
    {
        if(!parse_int(value, 0, INT_MAX, &apps[index].heartbeat_delay))
        {
            LOGE("Invalid heartbeat_delay for app %d: %s", index, value);
            return 0;
        }
    }
    else if(GEN_NAME("heartbeat_interval"), strcmp(name, expected_name) == 0)
    {
        if(!parse_int(value, 0, INT_MAX, &apps[index].heartbeat_interval))
        {
            LOGE("Invalid heartbeat_interval for app %d: %s", index, value);
            return 0;
        }
    }
    else if(GEN_NAME("cmd"), strcmp(name, expected_name) == 0)  // this always must be the last one
    {
        snprintf(apps[index].cmd, MAX_APP_CMD_LENGTH, "%s", value);

        if(strlen(value) >= MAX_APP_CMD_LENGTH)
        {
            LOGE("Invalid cmd for app %d - longer than %d charachters", index, MAX_APP_CMD_LENGTH);
            return 0;
        }

        app_state.ini_index++; // Move to next app after processing cmd
    }

    return 1;
}

int read_ini_file()
{
    memset(apps, 0, sizeof(apps));
    app_state.ini_index = 0;
    app_state.app_count = 0;
    app_state.uptime = get_uptime();
    app_state.udp_port = UDP_PORT;

    // ini_file could have already set by set_ini_file() earlier
    if(check_ini_file(app_state.ini_file))
    {
        LOGD("Using default ini file %s", INI_FILE);
        set_ini_file(INI_FILE);
    }

    LOGD("Reading ini file %s", app_state.ini_file);

    if(ini_parse(app_state.ini_file, handler, NULL) < 0)
    {
        LOGE("Failed to parse INI file %s", app_state.ini_file);
        return 1;
    }

    if(app_state.ini_index != app_state.app_count)
    {
        LOGW("Config mismatch: Expected %d apps, found %d", app_state.app_count, app_state.ini_index);
    }

    LOGD("%d processes have found in the ini file %s", app_state.app_count, app_state.ini_file);
    app_state.ini_last_modified_time = file_modified_time(app_state.ini_file);
    return 0;
}

//------------------------------------------------------------------

bool is_application_running(int i)
{
    if(apps[i].pid <= 0)
    {
        return false;
    }

    // Check if the application is running
    if(kill(apps[i].pid, 0) == 0)
    {
        return true;  // Process is running
    }
    else if(errno == EPERM)
    {
        LOGE("No permission to check if process %s is running : %s", apps[i].name, strerror(errno));
        return true;
    }
    else
    {
        LOGD("Process %s is not running : %s", apps[i].name, strerror(errno));
    }

    return false;
}

bool is_application_started(int i)
{
    return apps[i].started;
}

bool is_application_start_time(int i)
{
    return (get_uptime() - app_state.uptime) >= (long)apps[i].start_delay;
}

void start_application(int i)
{
    apps[i].pid = 0;
    // Start the application on Linux
    pid_t pid = fork();

    if(pid < 0)
    {
        LOGE("Failed to start process %s, error code: %d - %s", apps[i].name, errno, strerror(errno));
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
        LOGD("Starting the process %s with CMD : %s", apps[i].name, apps[i].cmd);
        run_command(apps[i].cmd);
        LOGE("Process %s stopped running", apps[i].name);
        exit(EXIT_FAILURE); // exit child process
    }
    else
    {
        // Parent process
        apps[i].started = true;
        apps[i].first_heartbeat = false;
        apps[i].pid = pid;
        LOGI("Process %s started (PID %d): %s", apps[i].name, apps[i].pid, apps[i].cmd);
        update_heartbeat_time(i);
    }
}

void kill_application(int i)
{
    bool killed = false;
    LOGD("Killing process %s", apps[i].name);

    if(apps[i].pid <= 0)
    {
        return;
    }

    if(kill(apps[i].pid, SIGTERM) < 0 && errno != ESRCH)
    {
        LOGE("Failed to terminate process %s, error: %d - %s", apps[i].name, errno, strerror(errno));
    }

    int status;
    int max_wait = MAX_WAIT_PROCESS_TERMINATION; // [seconds]
    LOGD("Waiting for the process %s", apps[i].name);

    do
    {
        sleep(1);
        int ret = waitpid(apps[i].pid, &status, WNOHANG | WUNTRACED | WCONTINUED);

        if(ret == 0)
        {
            LOGD("Process %s is still running", apps[i].name);
        }
        else if(ret < 0)
        {
            if(errno == ECHILD)
            {
                LOGD("Process %s already terminated", apps[i].name);
                max_wait = 0;
            }
            else
            {
                LOGE("Failed to wait for process %s, error: %d - %s", apps[i].name, errno, strerror(errno));
            }
        }
        else if(ret > 0)
        {
            if(WIFEXITED(status))
            {
                LOGD("Process %s exited, status=%d", apps[i].name, WEXITSTATUS(status));
                max_wait = 0;
            }
            else if(WIFSIGNALED(status))
            {
                LOGD("Process %s killed by signal %d", apps[i].name, WTERMSIG(status));
                max_wait = 0;
            }
            else if(WIFSTOPPED(status))
            {
                LOGD("Process %s stopped by signal %d", apps[i].name, WSTOPSIG(status));
                max_wait = 0;
            }
        }

        max_wait--;
    }
    while(max_wait > 0);

    if(is_application_running(i))
    {
        LOGD("Sending SIGKILL to process %s", apps[i].name);

        if(kill(apps[i].pid, SIGKILL) < 0 && errno != ESRCH)
        {
            LOGE("Failed to kill process %s, error: %d - %s", apps[i].name, errno, strerror(errno));
        }
        else
        {
            LOGI("Process %s killed", apps[i].name);

            if(!is_application_running(i))
            {
                killed = true;
            }
        }
    }
    else
    {
        LOGI("Process %s terminated", apps[i].name);
        killed = true;
    }

    if(killed)
    {
        apps[i].started = false;
        apps[i].first_heartbeat = false;
        apps[i].pid = 0;
    }
    else
    {
        LOGE("Failed to terminate process %s", apps[i].name);
    }
}

void restart_application(int i)
{
    LOGD("Restarting process %s", apps[i].name);

    if(is_application_running(i))
    {
        kill_application(i);
    }

    start_application(i);
    // Wait for the application to start
    int wait_time = 0;

    while(wait_time < MAX_WAIT_PROCESS_START)
    {
        sleep(1);

        if(is_application_running(i))
        {
            break;
        }

        wait_time++;
    }

    if(!is_application_running(i))
    {
        LOGE("Failed to start process %s", apps[i].name);
    }
    else
    {
        update_heartbeat_time(i);
        LOGI("Process %s restarted successfully", apps[i].name);
    }
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
