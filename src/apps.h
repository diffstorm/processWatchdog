/**
    @file apps.h
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

#ifndef APPS_H
#define APPS_H

#include <stdbool.h>
#include <time.h>

/**
    @file apps.h
    @brief Functions and structures for managing processes and applications defined in an INI config file.
*/

// Constants
#ifndef MAX_APPS
#define MAX_APPS 6 /**< Maximum supported number of applications. */
#endif
#define MAX_APP_CMD_LENGTH 256 /**< Maximum length of the command to start an application. */
#define MAX_APP_NAME_LENGTH 32 /**< Maximum length of an application name. */
#define MAX_WAIT_PROCESS_START 5 /**< Maximum time to wait for a process to start running (seconds). */
#define MAX_WAIT_PROCESS_TERMINATION 30 /**< Maximum time to wait for a process to terminate (seconds). */
#define INI_FILE "config.ini" /**< Default ini file path. */
#define UDP_PORT 12345 /**< Default udp port. */

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
    int max_retries; /**< Maximum number of retry attempts for restart policy. */
    int base_delay; /**< Base delay in seconds for exponential backoff. */
    int stop_timeout; /**< Seconds to wait for graceful shutdown before SIGKILL. */
    int progress_timeout; /**< Max seconds without progress signal. 0 = disabled. */
    // Not in the ini file
    bool started; /**< Flag indicating whether the application has been started. */
    bool first_heartbeat; /**< Flag indicating whether the application has sent its first heartbeat. */
    int pid; /**< Process ID of the application. */
    time_t last_heartbeat; /**< Time when the last heartbeat was received from the application. */
    unsigned long start_time; /**< Clock ticks since boot (field 22 of /proc/<pid>/stat). */
    int retry_count; /**< Current retry count for restart policy. */
    time_t last_failure_time; /**< Time when the last failure occurred. */
    time_t last_progress_time; /**< Monotonic time of last progress signal. */
    // Circuit-breaker
    int circuit_breaker_threshold; /**< 0 = disabled, N = restart after N failures in window */
    int circuit_breaker_window; /**< Window in seconds for failure counting (0 = default 300) */
    int circuit_state; /**< 0 = CLOSED, 1 = OPEN, 2 = HALF_OPEN */
    int failure_count; /**< Number of failures in current window */
    time_t window_start_time; /**< Start time of current failure window */
    // Cooldown
    int cooldown_period; /**< 0 = disabled, seconds to wait before auto-resume */
    int cooldown_count; /**< Number of cooldowns triggered */
    int cooldown_manual_ack_threshold; /**< 0 = auto-resume, N = require ack after N cooldowns */
    bool in_cooldown; /**< Flag indicating if in cooldown period */
    time_t cooldown_start_time; /**< Start time of current cooldown */
    time_t cooldown_end_time; /**< End time of current cooldown */
} Application_t;

typedef enum
{
    REBOOT_MODE_DISABLED,
    REBOOT_MODE_DAILY_TIME,
    REBOOT_MODE_INTERVAL
} RebootMode_t;

typedef struct
{
    int app_count; /**< Total number of applications found in the ini file. */
    int udp_port; /**< UDP port number specified in the ini file. */
    char ini_file[MAX_APP_CMD_LENGTH]; /**< Path to the ini file. */
    time_t ini_last_modified_time; /**< Last modified time of the ini file. */
    time_t uptime; /**< System uptime in seconds. */
    RebootMode_t periodic_reboot; /**< Periodic reboot mode. */
    union
    {
        struct
        {
            int hour;
            int min;
        } daily_time;
        long interval_minutes;
    } reboot_params; /**< Periodic reboot parameters. */
} AppState_t;

// Function prototypes

/**
    @brief Prints debug information for the specified application.

    @param i Index of the application.
*/
void print_app(int i);

/**
    @brief Finds the index of an application with the specified process ID.

    @param pid Process ID to search for.
    @return Index of the application if found, -1 otherwise.
*/
int find_pid(int pid);

/**
    @brief Gets direct access to the applications array for other modules.

    @return Pointer to the applications array.
*/
Application_t *apps_get_array(void);

/**
    @brief Gets direct access to the application state for other modules.

    @return Pointer to the application state structure.
*/
AppState_t *apps_get_state(void);

/**
    @brief Gets the total number of applications found in the ini file.

    @return Total number of applications.
*/
int get_app_count();

/**
    @brief Gets the name of the application at the specified index.

    @param i Index of the application.
    @return Name of the application.
*/
char *get_app_name(int i);

/**
    @brief Gets the process ID of the application at the specified index.

    @param i Index of the application.
    @return Process ID of the application.
*/
int get_app_pid(int i);

/**
    @brief Gets the UDP port number specified in the ini file.

    @return UDP port number.
*/
int get_udp_port();

/**
    @brief Sets the path to the ini file.

    @param path Path to the ini file.
    @return 0 if successful, -1 otherwise.
*/
int set_ini_file(char *path);

/**
    @brief Checks if the ini file has been modified since the last check.

    @return true if the ini file has been modified, false otherwise.
*/
bool is_ini_updated();

/**
    @brief Reads the information from the ini file and fills the Application_t structures accordingly.

    @return 0 if successful, -1 otherwise.
*/
int read_ini_file();

#endif // APPS_H
