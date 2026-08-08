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
#include "cmd.h"
#include "config.h"
#include "process.h"
#include "heartbeat.h"
#include "log.h"
#include "utils.h"

#include <libgen.h> // basename
#include <getopt.h>

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>

#define SOCKET_TIMEOUT  500 // [ms] poll timeout to wait for a UDP message blocking

void parse_commands(char *data, int length, int socket)
{
    // Use command module to parse the network command
    net_command_t cmd = cmd_parse_network(data, length);

    // Handle the parsed command based on its type
    switch(cmd.type)
    {
        case NET_CMD_HEARTBEAT:
        {
            int i = find_pid(cmd.pid);

            if(i >= 0)
            {
                time_t t = heartbeat_get_elapsed_time(i);

                if(heartbeat_get_first_received(i))
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
                    heartbeat_set_first_received(i);
                }

                heartbeat_update_time(i);

                if(cmd.has_progress_flag)
                {
                    Application_t *apps = apps_get_array();
                    Application_t *app = &apps[i];

                    if(app->progress_timeout > 0)
                    {
                        app->last_progress_time = (time_t)get_monotonic_time();
                        LOGD("Process %s progress signal received (timeout: %ds)",
                             app->name, app->progress_timeout);
                    }
                }
            }
        }
        break;

        case NET_CMD_START:
        {
            for(int i = 0; i < get_app_count(); i++)
            {
                if(0 == strncmp(get_app_name(i), cmd.app_name, MAX_APP_NAME_LENGTH - 1))
                {
                    if(!process_is_started(i))
                    {
                        process_start(i);
                        filecmd_remove_start(i);
                    }
                }
            }
        }
        break;

        case NET_CMD_STOP:
        {
            for(int i = 0; i < get_app_count(); i++)
            {
                if(0 == strncmp(get_app_name(i), cmd.app_name, MAX_APP_NAME_LENGTH - 1))
                {
                    if(process_is_running(i))
                    {
                        process_kill(i);
                        filecmd_create_stop(i);
                    }
                }
            }
        }
        break;

        case NET_CMD_RESTART:
        {
            for(int i = 0; i < get_app_count(); i++)
            {
                if(0 == strncmp(get_app_name(i), cmd.app_name, MAX_APP_NAME_LENGTH - 1))
                {
                    process_restart(i);
                    filecmd_remove_restart(i);
                }
            }
        }
        break;

        case NET_CMD_STATUS:
        {
            // Find app by name
            int app_index = -1;
            for(int i = 0; i < get_app_count(); i++)
            {
                if(0 == strncmp(get_app_name(i), cmd.app_name, MAX_APP_NAME_LENGTH - 1))
                {
                    app_index = i;
                    break;
                }
            }

            if(app_index >= 0)
            {
                Application_t *apps = apps_get_array();
                Application_t *app = &apps[app_index];

                // Determine status string
                const char *status = "unknown";
                if(!app->started)
                {
                    status = "stopped";
                }
                else if(process_is_running(app_index))
                {
                    status = "running";
                }
                else if(app->in_cooldown)
                {
                    status = "cooldown";
                }
                else if(app->circuit_state == 1) // OPEN
                {
                    status = "circuit_open";
                }
                else
                {
                    status = "crashed";
                }

                // Determine circuit state string
                const char *circuit = "unknown";
                if(app->circuit_breaker_threshold > 0)
                {
                    if(app->circuit_state == 0) // CLOSED
                        circuit = "closed";
                    else if(app->circuit_state == 1) // OPEN
                        circuit = "open";
                    else if(app->circuit_state == 2) // HALF_OPEN
                        circuit = "half_open";
                }
                else
                {
                    circuit = "disabled";
                }

                // Check if ack required
                bool ack_required = false;
                if(app->in_cooldown && app->cooldown_manual_ack_threshold > 0 &&
                   app->cooldown_count >= app->cooldown_manual_ack_threshold)
                {
                    ack_required = true;
                }

                // Calculate cooldown remaining
                int cooldown_remaining = 0;
                if(app->in_cooldown)
                {
                    long now = get_monotonic_time();
                    if(now < app->cooldown_end_time)
                    {
                        cooldown_remaining = (int)(app->cooldown_end_time - now);
                    }
                    else
                    {
                        cooldown_remaining = 0;
                    }
                }

                // Build response JSON-like string
                char response[256];
                snprintf(response, sizeof(response),
                         "{\"app\":\"%s\",\"status\":\"%s\",\"circuit\":\"%s\",\"cooldown\":%d,\"failures\":%d,\"ack_required\":%s}",
                         app->name, status, circuit, cooldown_remaining, app->failure_count,
                         ack_required ? "true" : "false");

                // Log and send response (for now, just log it)
                LOGN("Status response for %s: %s", app->name, response);
                
                // Send response to UDP client
                const char *sender_ip = udp_get_last_sender_ip();
                int sender_port = udp_get_last_sender_port();
                
                if(sender_port > 0)
                {
                    udp_send(socket, response, strlen(response), sender_ip, sender_port);
                    LOGN("Status response for %s sent to %s:%d", app->name, sender_ip, sender_port);
                }
                else
                {
                    LOGE("Status response failed: no sender address available");
                }
            }
            else
            {
                LOGE("Status command: app %s not found", cmd.app_name);
            }
        }
        break;

        case NET_CMD_UNKNOWN:
        default:
            // Error logging is already handled in cmd_parse_network
            break;
    }
}

//------------------------------------------------------------------

extern char *optarg;
extern int opterr, optind;

#define APPNAME     basename(argv[0])
#define VERSION     "1.4.5"
#define OPTSTR      "i:v:t:h"
#define USAGE_FMT   "%s -i <file.ini> [-v|--version] [-h|--help] [-t testname]\n"

static const struct option long_options[] =
{
    {"version", no_argument, NULL, 'v'},
    {"help",    no_argument, NULL, 'h'},
    {NULL,      0,           NULL,  0 }
};

static volatile sig_atomic_t kill_error = 10; // after 10 times SIGUSR1 the app exits forcefully
static volatile sig_atomic_t main_alive = 1; // terminate application (1=true, 0=false)
static volatile sig_atomic_t return_code = EXIT_NORMALLY;
static volatile sig_atomic_t caught_signal = 0; // signal that caused shutdown, 0 = none

// Handlers must stay async-signal-safe: set flags only (no logging/exit — iLOG locks a
// non-recursive mutex, so logging from signal context could self-deadlock). The main loop
// logs the cause after it exits.

// send signal INT to restart application
void SIGINT_handler(int sig)
{
    return_code = EXIT_RESTART;
    caught_signal = sig;
    main_alive = 0;
}

// send signal QUIT to reboot the system
void SIGQUIT_handler(int sig)
{
    return_code = EXIT_REBOOT;
    caught_signal = sig;
    main_alive = 0;
}

// send signal USR1 to terminate application
void SIGUSR1_handler(int sig)
{
    return_code = EXIT_NORMALLY;
    caught_signal = sig;
    main_alive = 0;

    if(kill_error > 0)
    {
        kill_error--;
    }
    else
    {
        _exit(EXIT_NORMALLY); // _exit, not exit: no atexit/stdio flush from signal context
    }
}

// rfu
void SIGUSR2_handler(int sig)
{
    UNUSED(sig);
}

void usage(char *progname, int opt)
{
    UNUSED(opt);
    fprintf(stderr, USAGE_FMT, progname);
}

void help(char *progname)
{
    fprintf(stderr, GREEN "\nBrief:\n" RESET);
    fprintf(stderr, "%s starts the applications given in the INI "
            "file in the same directory\n", progname);
    fprintf(stderr, "Restarts them when they crash or exit\n");
    fprintf(stderr, "The applications must send their PID numbers "
            "periodically to the UDP port in the INI file "
            "as a string command p<pid>, "
            "otherwise the %s will restart them.\n", progname);
    fprintf(stderr, GREEN "\nProgress Signal (optional):\n" RESET);
    fprintf(stderr, "progress_timeout = N  : Restart if no progress signal in N seconds\n");
    fprintf(stderr, "Send 'p<pid>+'          : When work completes\n");
    fprintf(stderr, "  p<pid>          : Liveness only (default)\n");
    fprintf(stderr, "  p<pid>+         : Liveness + progress\n");
    fprintf(stderr, GREEN "\nFile commands:\n" RESET
            "- start<app>\n"
            "- stop<app>\n"
            "- restart<app>\n"
            "- resume<app>\n"
            "- " FILECMD_STOPAPP "\n"
            "- " FILECMD_RESTARTAPP "\n"
            "- " FILECMD_REBOOT "\n");
    fprintf(stderr, GREEN "\nUDP commands:\n" RESET
            "- s<app>    : Get status of <app>\n"
            "  Response: JSON with status, circuit state, cooldown info, ack requirement\n");
    fprintf(stderr, GREEN "\nINI File example config:\n" RESET
            "[processWatchdog]\n"
            "udp_port = 12345\n"
            "periodic_reboot = OFF\n"
            "\n"
            "[app:Communicator]\n"
            "start_delay = 10\n"
            "heartbeat_delay = 60\n"
            "heartbeat_interval = 20\n"
            "cmd = /usr/bin/python test_child.py 1 crash\n"
            "\n"
            "[app:Bot]\n"
            "start_delay = 20\n"
            "heartbeat_delay = 90\n"
            "heartbeat_interval = 30\n"
            "progress_timeout = 60\n"
            "cmd = /usr/bin/python bot.py\n"
            "\n"
            "[app:Publisher]\n"
            "start_delay = 35\n"
            "heartbeat_delay = 70\n"
            "heartbeat_interval = 16\n"
            "cmd = /usr/bin/python test_child.py 3 crash\n"
            "\n"
            "[app:Alert]\n"
            "start_delay = 35\n"
            "heartbeat_delay = 130\n"
            "heartbeat_interval = 13\n"
            "cmd = /usr/bin/python test_child.py 4 noheartbeat\n"
            "\n"
            "# Circuit-breaker: 5 failures in 5 minutes opens circuit\n"
            "# circuit_breaker_threshold = 5\n"
            "# circuit_breaker_window = 300\n"
            "\n"
            "# Cooldown: wait 2 minutes before auto-resume\n"
            "# cooldown_period = 120\n"
            "# cooldown_manual_ack_threshold = 3\n"
            "\n"
            "# Restart policy with exponential backoff (optional)\n"
            "# max_retries = 5\n"
            "# base_delay = 1\n"
            "# stop_timeout = 30\n");
}

void version(char *progname)
{
    fprintf(stderr, "%s version : %s\n", progname, VERSION);
}

int main(int argc, char *argv[])
{
    int opt;
    opterr = 0;
    // sigaction (no SA_RESTART): a signal interrupts the UDP poll; udp_poll treats EINTR as
    // "no data", so the shutdown flags set by a handler are seen on the next loop iteration.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIGINT_handler; // restart
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);  // terminate
    sa.sa_handler = SIGQUIT_handler; // reboot
    sigaction(SIGQUIT, &sa, NULL);
    sa.sa_handler = SIGUSR1_handler; // terminate
    sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = SIGUSR2_handler; // rfu
    sigaction(SIGUSR2, &sa, NULL);

    // Scan parameters
    while((opt = getopt_long(argc, argv, OPTSTR, long_options, NULL)) != -1)
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
    time_t start_time = time(NULL);

    // Read config
    if(read_ini_file())
    {
        exit(EXIT_NORMALLY);
    }

    // Read statistics
    for(int i = 0; i < get_app_count(); i++)
    {
        stats_read_from_file(i, get_app_name(i));
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
        long uptime = time(NULL) - start_time;
        // Poll UDP messages
        length = sizeof(data) - 1;

        if(udp_poll(socket, SOCKET_TIMEOUT, data, &length))
        {
            if(main_alive)
            {
                LOGE("UDP poll failed, exiting for supervisor restart");
                return_code = EXIT_RESTART; // exit 2 -> run.sh re-runs us; 0 would be a silent permanent stop
                main_alive = 0;
                continue;
            }
        }
        else
        {
            if(length > 0)
            {
                data[length] = 0;
                parse_commands(data, length, socket);
            }
        }

        // Scan applications
        for(int i = 0; i < get_app_count(); i++)
        {
            if(process_is_started(i))
            {
                // Update resource usage stats (1 min)
                if((uptime % 60) == 0 && process_is_running(i))
                {
                    stats_update_cpu_usage(i, get_app_pid(i));
                    stats_update_memory_usage(i, get_app_pid(i));
                }

                // Update stats files periodically (15 mins)
                if((uptime % (15 * 60)) == 0)
                {
                    stats_write_to_file(i, get_app_name(i));
                    stats_print_to_file(i, get_app_name(i));
                }

                if(process_is_running(i))
                {
                    Application_t *apps = apps_get_array();
                    Application_t *app = &apps[i];

                    if(app->progress_timeout > 0)
                    {
                        // Validate progress_timeout is reasonable
                        // Must be > heartbeat_interval and not unreasonably long (> 7 days)
                        if(app->progress_timeout <= app->heartbeat_interval)
                        {
                            LOGW("Progress timeout %ds <= heartbeat interval %ds for %s, increasing to %ds",
                                 app->progress_timeout, app->heartbeat_interval, app->name,
                                 app->heartbeat_interval + 10);
                            app->progress_timeout = app->heartbeat_interval + 10;
                        }
                        else if(app->progress_timeout > 7 * 24 * 3600) // 7 days
                        {
                            LOGW("Progress timeout %ds > 7 days for %s, capping at 7 days",
                                 app->progress_timeout, app->name);
                            app->progress_timeout = 7 * 24 * 3600;
                        }

                        long now = get_monotonic_time();
                        time_t elapsed = (time_t)(now - app->last_progress_time);

                        if(elapsed > app->progress_timeout)
                        {
                            LOGE("Process %s has not sent a progress signal for %d seconds (timeout: %ds), restarting",
                                 app->name, elapsed, app->progress_timeout);
                            stats_heartbeat_reset_at(i);
                            process_restart(i);
                            // last_progress_time reset handled in process_restart
                            continue;
                        }

                        // Log progress signal reception for debugging
                        LOGD("Process %s progress OK: %ds since last signal (timeout: %ds)",
                             app->name, elapsed, app->progress_timeout);
                    }
                }

                if(!process_is_running(i))
                {
                    Application_t *apps = apps_get_array();
                    Application_t *app = &apps[i];
                    
                    LOGE("Process %s has crashed, restarting", get_app_name(i));
                    stats_crashed_at(i);
                    process_restart(i);
                    
                    // Log cooldown status if in cooldown
                    if(app->in_cooldown)
                    {
                        // cooldown_end_time is monotonic (process.c), so compare against the
                        // monotonic clock — not wall-clock time(NULL) (different epoch = garbage).
                        time_t remaining = app->cooldown_end_time - get_monotonic_time();
                        LOGN("Cooldown active for %s: %ds remaining",
                             app->name, (int)remaining);
                    }
                }
                else if(heartbeat_is_timeout(i))
                {
                    LOGE("Process %s has not sent a heartbeat in time, restarting", get_app_name(i));
                    stats_heartbeat_reset_at(i);
                    process_restart(i);
                }
                else if(filecmd_stop(i))
                {
                    LOGN("Process %s has stopped by file command", get_app_name(i));
                    process_kill(i);
                }
                else if(filecmd_restart(i))
                {
                    LOGN("Process %s has restarted by file command", get_app_name(i));
                    process_restart(i);
                    filecmd_remove_restart(i);
                }
                else if(filecmd_resume(i))
                {
                    Application_t *apps = apps_get_array();
                    Application_t *app = &apps[i];
                    
                    LOGN("Manual ack received for %s, resuming operations", get_app_name(i));
                    process_restart(i);
                    filecmd_remove_resume(i);
                    
                    // Reset cooldown state
                    app->in_cooldown = false;
                    app->cooldown_count = 0;
                    app->circuit_state = 0; // CLOSED
                }
            }
            else
            {
                if(!filecmd_stop(i) && (filecmd_start(i) || process_is_start_time(i)))
                {
                    process_start(i);

                    if(process_is_started(i))
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
            main_alive = 0;
            return_code = EXIT_NORMALLY;
        }
        else if(filecmd_exists(FILECMD_RESTARTAPP))
        {
            LOGN("%s has restarted by file command", APPNAME);
            main_alive = 0;
            return_code = EXIT_RESTART;
        }
        else if(filecmd_exists(FILECMD_REBOOT))
        {
            LOGN("System reboot by file command");
            main_alive = 0;
            return_code = EXIT_REBOOT;
        }

        // Check for periodic reboot (1 min)
        if(apps_get_state()->periodic_reboot != REBOOT_MODE_DISABLED && (uptime % 60 == 0))
        {
            switch(apps_get_state()->periodic_reboot)
            {
                case REBOOT_MODE_DAILY_TIME:
                {
                    time_t now = time(NULL);
                    struct tm tm_now;

                    // localtime_r + NULL guard (NULL would crash the tm_now.tm_hour deref)
                    if(localtime_r(&now, &tm_now) != NULL
                       && tm_now.tm_hour == apps_get_state()->reboot_params.daily_time.hour
                       && tm_now.tm_min == apps_get_state()->reboot_params.daily_time.min)
                    {
                        LOGN("Periodic reboot triggered (daily time)");
                        main_alive = 0;
                        return_code = EXIT_REBOOT;
                    }

                    break;
                }

                case REBOOT_MODE_INTERVAL:
                {
                    long uptime_minutes = uptime / 60;

                    if(uptime_minutes > 0 && uptime_minutes % apps_get_state()->reboot_params.interval_minutes == 0)
                    {
                        LOGN("Periodic reboot triggered (interval)");
                        main_alive = 0;
                        return_code = EXIT_REBOOT;
                    }

                    break;
                }

                default:
                    break;
            }
        }

#if 0 // feature disabled

        // Check if ini updated and re-read
        if(is_ini_updated())
        {
            if(read_ini_file())
            {
                // TODO: attaching to existing PIDs to prevent restarting the processes
                exit(EXIT_RESTART);
            }
        }

#endif
    }

    // Log a signal-triggered shutdown here (handlers stay async-signal-safe). Non-signal
    // exits (UDP failure, file commands, periodic reboot) already log their own cause.
    if(caught_signal != 0)
    {
        LOGN("Signal %d detected, shutting down (return code %d)", (int)caught_signal, return_code);
    }

    LOGD("%s ending...", APPNAME);
    // Stop UDP server
    udp_stop(socket);

    for(int i = 0; i < get_app_count(); i++)
    {
        // Update stats files
        stats_write_to_file(i, get_app_name(i));
        stats_print_to_file(i, get_app_name(i));
        // Kill running applications
        process_kill(i);

        if(!process_is_running(i))
        {
            LOGN("Process %s has ended", get_app_name(i));
        }
    }

    LOGN("%s ended with return code %d", APPNAME, return_code);
    return return_code;
}
