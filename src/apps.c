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
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
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

static Application_t apps[MAX_APPS]; /**< Array of Application_t structures representing applications defined in the ini file. */
static int app_count; /**< Total number of applications found in the ini file. */
static int udp_port = 12345; /**< UDP port number specified in the ini file. */
static char ini_file[MAX_APP_CMD_LENGTH] = INI_FILE; /**< Path to the ini file. */
static time_t ini_last_modified_time; /**< Last modified time of the ini file. */
static long uptime; /**< System uptime in seconds. */
static int ini_index; /**< Index used to read an array in the ini file. */

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
    for(int i = 0; i < app_count; i++)
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

int set_ini_file(char *path)
{
    int ret = 1; // fail
    int length = strlen(path);

    if(NULL != path && 0 < length && MAX_APP_CMD_LENGTH > length)
    {
        if(f_exist(path))
        {
            strncpy(ini_file, path, MAX_APP_CMD_LENGTH);
            LOGD("ini file has been set to %s", ini_file);
            ret = 0; // success
        }
    }

    if(ret)
    {
        LOGE("Error setting ini file %s", path);
    }

    return ret;
}

static time_t file_modified_time(char *path)
{
    struct stat attr;
    stat(path, &attr);
    return attr.st_mtime;
}

bool is_ini_updated()
{
    time_t file_last_modified_time = file_modified_time(ini_file);
    return (file_last_modified_time != ini_last_modified_time);
}

static int handler(void *user, const char *section, const char *name, const char *value)
{
    (void)(user);
    const char *_section = "processWatchdog";
    char b[INI_MAX_LINE];
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
#define SECTION(n, s) snprintf(b, sizeof(b) - 1, "%d_%s", n + 1, s);

    if(MATCH(_section, "udp_port"))
    {
        udp_port = atoi(value);
    }

    if(MATCH(_section, "nWdtApps"))
    {
        app_count = atoi(value);
    }

    if(app_count > 0)
    {
        SECTION(ini_index, "name");

        if(MATCH(_section, b))
        {
            int length = strlen(value);

            if(length > MAX_APP_NAME_LENGTH)
            {
                LOGE("NAME is longer than %d", MAX_APP_NAME_LENGTH);
            }

            length = length > MAX_APP_NAME_LENGTH ? MAX_APP_NAME_LENGTH : length;
            strncpy(apps[ini_index].name, value, length);
        }

        SECTION(ini_index, "start_delay");

        if(MATCH(_section, b))
        {
            apps[ini_index].start_delay = atoi(value);
        }

        SECTION(ini_index, "heartbeat_delay");

        if(MATCH(_section, b))
        {
            apps[ini_index].heartbeat_delay = atoi(value);
        }

        SECTION(ini_index, "heartbeat_interval");

        if(MATCH(_section, b))
        {
            apps[ini_index].heartbeat_interval = atoi(value);
        }

        SECTION(ini_index, "cmd"); // this always must be the last one

        if(MATCH(_section, b))
        {
            int length = strlen(value);

            if(length > MAX_APP_CMD_LENGTH)
            {
                LOGE("CMD is longer than %d", MAX_APP_CMD_LENGTH);
            }

            length = length > MAX_APP_CMD_LENGTH ? MAX_APP_CMD_LENGTH : length;
            strncpy(apps[ini_index].cmd, value, length);
            ini_index++; // order of the names in the ini are important
        }
    }

    return 1;
}

int read_ini_file()
{
    uptime = get_uptime();
    LOGD("Reading ini file %s", ini_file);
    memset(apps, 0, sizeof(apps));
    app_count = 0;
    ini_index = 0;

    if(ini_parse(ini_file, handler, NULL) < 0)
    {
        LOGE("Can't load %s", ini_file);
        return 1;
    }

    LOGD("%d processes have found in the ini file %s", app_count, ini_file);
    ini_last_modified_time = file_modified_time(ini_file);
    return 0;
}

//------------------------------------------------------------------

bool is_application_running(int i)
{
    pid_t result = -1;

    if(apps[i].pid > 0)
    {
        // Check if the application is running on Linux
        if(kill(apps[i].pid, 0) == 0)
        {
            //LOGD("Process %s is running", apps[i].name);
            /* process is running or a zombie */
            result = 0;
        }
        else
        {
            LOGD("Process %s is not running : %s", apps[i].name, strerror(errno));
        }
    }

    return (result == 0);
}

bool is_application_started(int i)
{
    return apps[i].started;
}

bool is_application_start_time(int i)
{
    return (get_uptime() - uptime) >= (long)apps[i].start_delay;
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
        /* Delete signal handlers */
        signal(SIGINT, SIG_DFL); // restart
        signal(SIGTERM, SIG_DFL); // terminate
        signal(SIGQUIT, SIG_DFL); // reboot
        signal(SIGUSR1, SIG_DFL); // terminate
        signal(SIGUSR2, SIG_DFL); // rfu
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

    // Send the SIGTERM signal to the application on Linux
    if(kill(apps[i].pid, SIGTERM) < 0)
    {
        if(errno != ESRCH) // No such process
        {
            LOGE("Failed to terminate process %s, error: %d - %s", apps[i].name, errno, strerror(errno));
        }
    }

    // Wait for the process to terminate
    int status;
    LOGD("Waiting for the process %s", apps[i].name);
    int max_wait = MAX_WAIT_PROCESS_TERMINATION; // [seconds]

    do
    {
        sleep(1);

        if(waitpid(apps[i].pid, &status, WUNTRACED | WCONTINUED) < 0)
        {
            if(errno != ECHILD)
            {
                LOGE("Failed to wait for process %s, error : %d - %s", apps[i].name, errno, strerror(errno));
            }
        }

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
        else if(WIFCONTINUED(status))
        {
            LOGD("Process %s continued", apps[i].name);
            max_wait--;
        }
    }
    while(0 < max_wait);

    sleep(1);

    // If the process hasn't terminated after receiving SIGTERM, send the SIGKILL signal
    if(is_application_running(i))
    {
        LOGD("Sending SIGKILL to process %s", apps[i].name);

        if(kill(apps[i].pid, SIGKILL) < 0)
        {
            if(errno != ESRCH) // No such process
            {
                LOGE("Failed to kill process %s, error : %d - %s", apps[i].name, errno, strerror(errno));
            }
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
}

void restart_application(int i)
{
    // Log that the application is being restarted
    LOGD("Restarting process %s", apps[i].name);

    // Kill the existing instance of the application
    if(is_application_running(i))
    {
        kill_application(i);
    }

    // Start a new instance of the application
    start_application(i);
    // Wait for the new instance of the application to start
    sleep(2);

    // Check if the new instance of the application is running
    if(!is_application_running(i))
    {
        LOGE("Failed to start process %s", apps[i].name);
    }
    else
    {
        // Update the last_heartbeat time to prevent immediate restart
        update_heartbeat_time(i);
        // Log that the application has been successfully restarted
        LOGI("Process %s restarted successfully", apps[i].name);
    }
}

int get_app_count(void)
{
    return app_count;
}

char *get_app_name(int i)
{
    return apps[i].name;
}

int get_udp_port(void)
{
    return udp_port;
}
