/**
    @file test.h
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

#ifndef TEST_H
#define TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/**
    @file test.h
    @brief Functionality tests reachable over application arguments.
*/

/**
    @brief Run a test

    @param Test name.
*/
void test(char *testname);

#ifdef __cplusplus
}
#endif

#endif /* test.h */
