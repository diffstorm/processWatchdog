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
    @brief Functions and structures for managing processes and applications defined in an ini config file.
*/

// Constants
#define MAX_APPS 6 /**< Maximum supported number of applications. */
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
} AppState_t;

// Function prototypes

/**
    @brief Prints debug information for the specified application.

    @param i Index of the application.
*/
void print_app(int i);

/**
    @brief Updates the last heartbeat time for the specified application.

    @param i Index of the application.
*/
void update_heartbeat_time(int i);

/**
    @brief Finds the index of an application with the specified process ID.

    @param pid Process ID to search for.
    @return Index of the application if found, -1 otherwise.
*/
int find_pid(int pid);

/**
    @brief Gets the elapsed time since the last heartbeat received from the specified application.

    @param i Index of the application.
    @return Elapsed time in seconds.
*/
time_t get_heartbeat_time(int i);

/**
    @brief Checks if it is time to expect a heartbeat from the specified application.

    @param i Index of the application.
    @return true if it is time to expect a heartbeat, false otherwise.
*/
bool is_timeup(int i);

/**
    @brief Sets the flag indicating that the specified application has sent its first heartbeat.

    @param i Index of the application.
*/
void set_first_heartbeat(int i);

/**
    @brief Gets the flag indicating whether the specified application has sent its first heartbeat.

    @param i Index of the application.
    @return true if the application has sent its first heartbeat, false otherwise.
*/
bool get_first_heartbeat(int i);

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

/**
    @brief Checks if the specified application is currently running.

    @param i Index of the application.
    @return true if the application is running, false otherwise.
*/
bool is_application_running(int i);

/**
    @brief Checks if the specified application has been started.

    @param i Index of the application.
    @return true if the application has been started, false otherwise.
*/
bool is_application_started(int i);

/**
    @brief Checks if it is time to start the specified application based on the start delay.

    @param i Index of the application.
    @return true if it is time to start the application, false otherwise.
*/
bool is_application_start_time(int i);

/**
    @brief Starts the specified application.

    @param i Index of the application.
*/
void start_application(int i);

/**
    @brief Stops the specified application by killing its process.

    @param i Index of the application.
*/
void kill_application(int i);

/**
    @brief Restarts the specified application.

    @param i Index of the application.
*/
void restart_application(int i);

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
    @brief Gets the UDP port number specified in the ini file.

    @return UDP port number.
*/
int get_udp_port();

#endif // APPS_H