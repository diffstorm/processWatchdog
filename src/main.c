/**
    @file main.c
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

#include "server.h"
#include "apps.h"
#include "filecmd.h"
#include "stats.h"
#include "test.h"
#include "log.h"
#include "utils.h"

#include <libgen.h> // basename
#include <getopt.h>

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>

#define SOCKET_TIMEOUT  500 // [ms] poll timeout to wait for a UDP message blocking

void parse_commands(char *data, int length)
{
    switch(data[0])
    {
        case 'p': // pid heartbeat : p<pid> ? p1234
        {
            int n = parse_number(data, length, NULL);
            LOGD("Heartbeat command received from pid %d : %s", n, data);

            if(0 < n && INT32_MAX > n)
            {
                int i = find_pid(n);

                if(i >= 0)
                {
                    time_t t = get_heartbeat_time(i);

                    if(get_first_heartbeat(i))
                    {
                        if(t >= 0)
                        {
                            LOGD("%s heartbeat after %d seconds", get_app_name(i), t);
                            stats_update_heartbeat_time(i, t);
                        }
                    }
                    else
                    {
                        LOGD("%s first heartbeat after %d seconds", get_app_name(i), t);
                        stats_update_first_heartbeat_time(i, t);
                        set_first_heartbeat(i);
                    }

                    update_heartbeat_time(i);
                }
            }
            else
            {
                LOGE("Invalid pid received, pid %d : %s", n, data);
            }
        }
        break;
#if 0 // feature disabled

        case 'a': // stArt : a<name> ? aBot
        {
            LOGD("Start command received: %s", data);
            char name[MAX_APP_NAME_LENGTH];
            strncpy(name, &data[1], MAX_APP_NAME_LENGTH - 1);

            for(int i = 0; i < get_app_count(); i++)
            {
                if(0 == strncmp(get_app_name(i), name, MAX_APP_NAME_LENGTH - 1))
                {
                    if(!is_application_started(i))
                    {
                        start_application(i);
                        filecmd_remove_start(i);
                    }
                }
            }
        }
        break;

        case 'o': // stOp : o<name> ? oBot
        {
            LOGD("Stop command received: %s", data);
            char name[MAX_APP_NAME_LENGTH];
            strncpy(name, &data[1], MAX_APP_NAME_LENGTH - 1);

            for(int i = 0; i < get_app_count(); i++)
            {
                if(0 == strncmp(get_app_name(i), name, MAX_APP_NAME_LENGTH - 1))
                {
                    if(is_application_running(i))
                    {
                        kill_application(i);
                        filecmd_create_stop(i);
                    }
                }
            }
        }
        break;

        case 'r': // Restart : r<name> ? rBot
        {
            LOGD("Restart command received: %s", data);
            char name[MAX_APP_NAME_LENGTH];
            strncpy(name, &data[1], MAX_APP_NAME_LENGTH - 1);

            for(int i = 0; i < get_app_count(); i++)
            {
                if(0 == strncmp(get_app_name(i), name, MAX_APP_NAME_LENGTH - 1))
                {
                    restart_application(i);
                    filecmd_remove_restart(i);
                }
            }
        }
        break;
#endif

        default:
        {
            const int max_bytes = MAX_APP_NAME_LENGTH;
            const int max_length = max_bytes * 3;
            char hexStr[max_length];
            memset(hexStr, 0, sizeof(hexStr));

            if (length > max_bytes) {
                length = max_bytes;
            }

            for (int i = 0; i < length; i++) {
                snprintf(&hexStr[i * 3], sizeof(hexStr) - (i * 3), "%02X ", data[i]);
            }

            char printableStr[max_bytes + 1];
            memset(printableStr, 0, sizeof(printableStr));
            for (int i = 0; i < length; i++) {
                printableStr[i] = (data[i] >= 32 && data[i] < 127) ? data[i] : '.';
            }

            LOGE("Unknown command received : %s | %s", printableStr, hexStr);
        }
        break;
    }
}

//------------------------------------------------------------------

extern char *optarg;
extern int opterr, optind;

#define APPNAME     basename(argv[0])
#define VERSION     "1.1.0"
#define OPTSTR      "i:v:t:h"
#define USAGE_FMT   "%s -i <file.ini> [-v] [-h] [-t testname]\n"

static volatile int kill_error = 10; // after 10 times SIGUSR1 the app exits forcefully
static volatile bool main_alive = true; // terminate application
static volatile int return_code = EXIT_NORMALLY;

// send signal INT to restart application
void SIGINT_handler(int sig)
{
    UNUSED(sig);
    LOGN("INT detected, Restarting");
    main_alive = false;
    return_code = EXIT_RESTART;
}

// send signal QUIT to reboot the system
void SIGQUIT_handler(int sig)
{
    UNUSED(sig);
    LOGN("QUIT detected, Rebooting");
    main_alive = false;
    return_code = EXIT_REBOOT;
}

// send signal USR1 to terminate application
void SIGUSR1_handler(int sig)
{
    UNUSED(sig);
    LOGN("USR1 detected, Terminating");
    main_alive = false;
    return_code = EXIT_NORMALLY;

    if(kill_error > 0)
    {
        kill_error--;
    }
    else
    {
        LOGE("10x USR1 detected, Terminating forcefully");
        exit(EXIT_NORMALLY);
    }
}

// rfu
void SIGUSR2_handler(int sig)
{
    UNUSED(sig);
    LOGN("USR2 detected");
}

void usage(char *progname, int opt)
{
    UNUSED(opt);
    fprintf(stderr, USAGE_FMT, progname);
}

void help(char *progname)
{
    fprintf(stderr, GREEN "\nBrief:\n" RESET);
    fprintf(stderr, "%s starts the applications given in the ini "
            "file in the same directory\n", progname);
    fprintf(stderr, "Restarts them when they crash or exit\n");
    fprintf(stderr, "The applications must send their pid numbers "
            "periodically to the UDP port in the ini file "
            "as a string command p<pid>, "
            "otherwise the %s will restart them.\n", progname);
    fprintf(stderr, GREEN "\nFile commands:\n" RESET
            "- start<app>\n"
            "- stop<app>\n"
            "- restart<app>\n"
            "- " FILECMD_STOPAPP "\n"
            "- " FILECMD_RESTARTAPP "\n"
            "- " FILECMD_REBOOT "\n");
    fprintf(stderr, GREEN "\nINI File example config:\n" RESET
            "[processWatchdog]\n"
            "udp_port = 12345\n"
            "n_apps = 4\n"
            "1_name = App1\n"
            "1_start_delay = 10\n"
            "1_heartbeat_delay = 60\n"
            "1_heartbeat_interval = 20\n"
            "1_cmd = /usr/bin/python test_child.py 1 crash\n"
            "2_name = App2\n"
            "2_start_delay = 20\n"
            "2_heartbeat_delay = 90\n"
            "2_heartbeat_interval = 30\n"
            "2_cmd = /usr/bin/python test_child.py 2 noheartbeat\n"
            "3_name = App3\n"
            "3_start_delay = 35\n"
            "3_heartbeat_delay = 70\n"
            "3_heartbeat_interval = 16\n"
            "3_cmd = /usr/bin/python test_child.py 3 crash\n"
            "4_start_delay = 35\n"
            "4_name = App4\n"
            "4_heartbeat_delay = 130\n"
            "4_heartbeat_interval = 13\n"
            "4_cmd = /usr/bin/python test_child.py 4 noheartbeat\n");
}

void version(char *progname)
{
    fprintf(stderr, "%s version : %s\n", progname, VERSION);
}

int main(int argc, char *argv[])
{
    int opt;
    opterr = 0;
    // Setup signal handlers
    signal(SIGINT, SIGINT_handler); // restart
    signal(SIGTERM, SIGINT_handler); // terminate
    signal(SIGQUIT, SIGQUIT_handler); // reboot
    signal(SIGUSR1, SIGUSR1_handler); // terminate
    signal(SIGUSR2, SIGUSR2_handler); // rfu

    // Scan parameters
    while((opt = getopt(argc, argv, OPTSTR)) != EOF)
    {
        switch(opt)
        {
            case 'i': // ini file path
                if(set_ini_file(optarg))
                {
                    exit(EXIT_NORMALLY);
                }

                break;

            case 't': // unit tests
                test(optarg);
                exit(EXIT_NORMALLY);
                break;

            case 'v': // version
                version(APPNAME);
                exit(EXIT_NORMALLY);
                break;

            case 'h': // help
                usage(APPNAME, opt);
                help(APPNAME);
                exit(EXIT_NORMALLY);
                break;

            default:
                break;
        }
    }

    LOGN("%s started v:%s", APPNAME, VERSION);

    // Read config
    if(read_ini_file())
    {
        exit(EXIT_NORMALLY);
    }

    // Read statistics
    for(int i = 0; i < get_app_count(); i++)
    {
        stats_read_from_file(i);
    }

    // data buffer
    char data[MAX_APP_CMD_LENGTH];
    int length;
    // Start UDP server
    int socket;

    if(udp_start(&socket, get_udp_port()))
    {
        LOGE("UDP start failed");
        udp_stop(socket);
        exit(EXIT_RESTART);
    }

    // Loop here until exit signal arrived
    while(main_alive)
    {
        // Poll UDP messages
        length = sizeof(data) - 1;

        if(udp_poll(socket, SOCKET_TIMEOUT, data, &length))
        {
            if(main_alive)
            {
                LOGE("UDP poll failed");
                main_alive = false;
                continue;
            }
        }
        else
        {
            if(length > 0)
            {
                data[length] = 0;
                parse_commands(data, length);
            }
        }

        // Scan applications
        for(int i = 0; i < get_app_count(); i++)
        {
            if(is_application_started(i))
            {
                // Update stats files periodically (15 mins)
                if((get_uptime() % (15 * 60)) == 0)
                {
                    stats_write_to_file(i);
                    stats_print_to_file(i);
                }

                if(!is_application_running(i))
                {
                    LOGE("Process %s has crashed, restarting", get_app_name(i));
                    stats_crashed_at(i);
                    restart_application(i);
                }
                else if(is_timeup(i))
                {
                    LOGE("Process %s has not sent a heartbeat in time, restarting", get_app_name(i));
                    stats_heartbeat_reset_at(i);
                    restart_application(i);
                }
                else if(filecmd_stop(i))
                {
                    LOGN("Process %s has stopped by file command", get_app_name(i));
                    kill_application(i);
                }
                else if(filecmd_restart(i))
                {
                    LOGN("Process %s has restarted by file command", get_app_name(i));
                    restart_application(i);
                    filecmd_remove_restart(i);
                }
            }
            else
            {
                if(!filecmd_stop(i) && (filecmd_start(i) || is_application_start_time(i)))
                {
                    start_application(i);

                    if(is_application_started(i))
                    {
                        LOGN("Process %s has started", get_app_name(i));
                        stats_started_at(i);
                        filecmd_remove_start(i);
                        filecmd_remove_restart(i);
                    }
                }
            }
        }

        // Check for general purpose file commands
        if(filecmd_exists(FILECMD_STOPAPP))
        {
            LOGN("%s has stopped by file command", APPNAME);
            main_alive = false;
            return_code = EXIT_NORMALLY;
        }
        else if(filecmd_exists(FILECMD_RESTARTAPP))
        {
            LOGN("%s has restarted by file command", APPNAME);
            main_alive = false;
            return_code = EXIT_RESTART;
        }
        else if(filecmd_exists(FILECMD_REBOOT))
        {
            LOGN("System reboot by file command");
            main_alive = false;
            return_code = EXIT_REBOOT;
        }

#if 0 // feature disabled

        // Check if ini updated and re-read
        if(is_ini_updated())
        {
            if(read_ini_file())
            {
                exit(EXIT_RESTART);
            }
        }

#endif
    }

    LOGD("%s ending...", APPNAME);
    // Stop UDP server
    udp_stop(socket);

    for(int i = 0; i < get_app_count(); i++)
    {
        // Update stats files
        stats_write_to_file(i);
        stats_print_to_file(i);
        // Kill running applications
        kill_application(i);

        if(!is_application_running(i))
        {
            LOGN("Process %s has ended", get_app_name(i));
        }
    }

    LOGN("%s ended with return code %d", APPNAME, return_code);
    return return_code;
}
