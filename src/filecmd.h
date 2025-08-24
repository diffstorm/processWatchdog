/**
    @file filecmd.h
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

#ifndef FILECMD_H
#define FILECMD_H

#include <stdbool.h>

/**
    @file filecmd.h
    @brief File commands for controlling the application and managing application-specific commands.
*/

/*
    File commands to control the application itself
*/
#define FILECMD_STOPAPP     "wdtstop" /**< Command to stop all apps and then itself. */
#define FILECMD_RESTARTAPP  "wdtrestart" /**< Command to stop all apps and restart itself. */
#define FILECMD_REBOOT      "wdtreboot" /**< Command to stop all apps and reboot the OS. */

/**
    @brief File commands for controlling the application lifecycle based on an application's name specified in the INI file:

    @example for an application called "bot" or "Bot"
    - stopbot: Stops the application named "bot". It does not restart the application automatically. When the corresponding file is removed, the "bot" application will be started again.

    - startbot: If the "bot" application is not currently running, this command will start it. Upon successful start, the "startbot" file will be removed.

    - restartbot: Restarts the "bot" application if it is already running. The "restartbot" file will be removed after the restart operation is completed.
*/

/**
    @brief Checks if the file command to start an application exists.

    @param i Index of the application.
    @return true if the file command exists, false otherwise.
*/
bool filecmd_start(int i);

/**
    @brief Checks if the file command to stop an application exists.

    @param i Index of the application.
    @return true if the file command exists, false otherwise.
*/
bool filecmd_stop(int i);

/**
    @brief Checks if the file command to restart an application exists.

    @param i Index of the application.
    @return true if the file command exists, false otherwise.
*/
bool filecmd_restart(int i);

/**
    @brief Removes the file command to start an application if it exists.

    @param i Index of the application.
*/
void filecmd_remove_start(int i);

/**
    @brief Removes the file command to stop an application if it exists.

    @param i Index of the application.
*/
void filecmd_remove_stop(int i);

/**
    @brief Removes the file command to restart an application if it exists.

    @param i Index of the application.
*/
void filecmd_remove_restart(int i);

/**
    @brief Creates the file command to start an application if it does not exist.

    @param i Index of the application.
*/
void filecmd_create_start(int i);

/**
    @brief Creates the file command to stop an application if it does not exist.

    @param i Index of the application.
*/
void filecmd_create_stop(int i);

/**
    @brief Creates the file command to restart an application if it does not exist.

    @param i Index of the application.
*/
void filecmd_create_restart(int i);

/**
    @brief Checks if a file exists and removes it.

    @param fname Name of the file.
    @return true if the file exists and is removed successfully, false otherwise.
*/
bool filecmd_exists(const char *fname);

#endif // FILECMD_H
