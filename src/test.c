/**
    @file stats.c
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
#include "config.h"
#include "server.h"
#include "filecmd.h"
#include "log.h"
#include "utils.h"
#include "cmd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* internal macros */
#define cmp(x)  if(0 == strcmp(testname, x))
#define chk(x)  printf("%s\n", false != x() ? "Success" : "Fail!");

int test_heartbeat_parsing()
{
    net_command_t cmd;
    char data[32];
    int pass = 1;
    // Test standard heartbeat (no progress)
    snprintf(data, sizeof(data), "p12345");
    cmd = cmd_parse_network(data, strlen(data));

    if(cmd.type != NET_CMD_HEARTBEAT || cmd.pid != 12345 || cmd.has_progress_flag != 0)
    {
        printf("Standard heartbeat failed\n");
        pass = 0;
    }

    // Test heartbeat with progress signal
    snprintf(data, sizeof(data), "p12345+");
    cmd = cmd_parse_network(data, strlen(data));

    if(cmd.type != NET_CMD_HEARTBEAT || cmd.pid != 12345 || cmd.has_progress_flag != 1)
    {
        printf("Heartbeat with progress failed\n");
        pass = 0;
    }

    // Test heartbeat with progress signal (different PID)
    snprintf(data, sizeof(data), "p9999+");
    cmd = cmd_parse_network(data, strlen(data));

    if(cmd.type != NET_CMD_HEARTBEAT || cmd.pid != 9999 || cmd.has_progress_flag != 1)
    {
        printf("Heartbeat with progress (PID 9999) failed\n");
        pass = 0;
    }

    // 'p' with no digit (drove the pre-fix OOB skip loop); not NUL-terminated, bounded by length
    char raw[8] = { 'p', 'x', 'y', 'z', 'q', 'r', 's', 't' };
    cmd = cmd_parse_network(raw, (int)sizeof(raw));

    if(cmd.type != NET_CMD_UNKNOWN)
    {
        printf("Malformed 'p' with no digit was not rejected\n");
        pass = 0;
    }

    // High-bit bytes after 'p' (isdigit on a negative char was UB pre-fix)
    char high[4] = { 'p', (char)0x80, (char)0xFF, 'z' };
    cmd = cmd_parse_network(high, (int)sizeof(high));

    if(cmd.type != NET_CMD_UNKNOWN)
    {
        printf("Malformed 'p' with high-bit bytes was not rejected\n");
        pass = 0;
    }

    return pass;
}

void test_filecmd()
{
    if(!read_ini_file())
    {
        // TODO
    }
}

void test_config()
{
    if(read_ini_file())
    {
        printf("Error on reading the ini\n");
    }

    for(int i = 0; i < get_app_count(); i++)
    {
        print_app(i);
    }
}

void test_log()
{
    for(int i = 0; i < (int)LOG_PRIORITY_MAX; ++i)
    {
        LOGE("LOG test iteration %d", i);
    }
}

void test_delay()
{
    int ms = 4500;
    clk_t t = time_ms();
    delay_ms(ms);
    t = elapsed_ms(t);
    printf("Waited\t\t%d ms\nMeasured\t%llu ms\n", ms, t);
}

void test_exit_normal()
{
    printf("Exit normal\n");
    exit(EXIT_NORMALLY);
}

void test_exit_crash()
{
    printf("Exit crash\n");
    exit(EXIT_CRASHED);
}

void test_exit_restart()
{
    printf("Exit restart\n");
    exit(EXIT_RESTART);
}

void test_exit_reboot()
{
    printf("Exit reboot\n");
    exit(EXIT_REBOOT);
}

void test_exit_unknown()
{
    printf("Exit unknown\n");
    exit(123);
}

int test_circuit_breaker()
{
    int pass = 1;
    
    // Test circuit-breaker default values
    Application_t app = {0};
    app.circuit_breaker_threshold = 5;
    app.circuit_breaker_window = 300;
    app.circuit_state = 0; // CLOSED
    app.failure_count = 0;
    app.window_start_time = 0;
    
    // Verify defaults
    if(app.circuit_state != 0 || app.failure_count != 0)
    {
        printf("Circuit-breaker initial state failed\n");
        pass = 0;
    }
    
    return pass;
}

int test_cooldown()
{
    int pass = 1;
    
    // Test cooldown default values
    Application_t app = {0};
    app.cooldown_period = 120;
    app.cooldown_manual_ack_threshold = 3;
    app.cooldown_count = 0;
    app.in_cooldown = false;
    
    // Verify defaults
    if(app.cooldown_period != 120 || app.cooldown_manual_ack_threshold != 3)
    {
        printf("Cooldown initial state failed\n");
        pass = 0;
    }
    
    return pass;
}

/*
    Regression test: a configured progress_timeout must survive config_parse_file (it was
    once reset to 0 after parsing), while an app omitting the key must default to 0.
*/
int test_progress_timeout_config()
{
    int pass = 1;
    const char *path = "test_progress_timeout.ini";

    FILE *f = fopen(path, "w");
    if(!f)
    {
        printf("progress_timeout config: could not create temp ini\n");
        return 0;
    }
    fprintf(f,
            "[processWatchdog]\n"
            "udp_port = 12345\n"
            "\n"
            "[app:WithProgress]\n"
            "start_delay = 5\n"
            "heartbeat_delay = 10\n"
            "heartbeat_interval = 15\n"
            "progress_timeout = 30\n"
            "cmd = /bin/true\n"
            "\n"
            "[app:NoProgress]\n"
            "start_delay = 5\n"
            "heartbeat_delay = 10\n"
            "heartbeat_interval = 15\n"
            "cmd = /bin/true\n");
    fclose(f);

    Application_t apps[MAX_APPS];
    AppState_t state = {0};

    if(config_parse_file(path, apps, MAX_APPS, &state) != 0)
    {
        printf("progress_timeout config: parse failed\n");
        remove(path);
        return 0;
    }

    if(state.app_count != 2)
    {
        printf("progress_timeout config: expected 2 apps, got %d\n", state.app_count);
        pass = 0;
    }

    for(int i = 0; i < state.app_count; i++)
    {
        if(strcmp(apps[i].name, "WithProgress") == 0 && apps[i].progress_timeout != 30)
        {
            printf("progress_timeout config: WithProgress expected 30, got %d (B1 regression)\n",
                   apps[i].progress_timeout);
            pass = 0;
        }
        if(strcmp(apps[i].name, "NoProgress") == 0 && apps[i].progress_timeout != 0)
        {
            printf("progress_timeout config: NoProgress expected 0, got %d\n",
                   apps[i].progress_timeout);
            pass = 0;
        }
    }

    remove(path);
    return pass;
}

void test(char *testname)
{
    if(0 == strlen(testname))
    {
        printf("Invalid testname!\n");
        return;
    }

    printf("\nTest: %s\n", testname);
    cmp("filecmd")
    {
        test_filecmd();
    }
    cmp("config")
    {
        test_config();
    }
    cmp("log")
    {
        test_log();
    }
    cmp("delay")
    {
        test_delay();
    }
    cmp("exit_normal")
    {
        test_exit_normal();
    }
    cmp("exit_crash")
    {
        test_exit_crash();
    }
    cmp("exit_restart")
    {
        test_exit_restart();
    }
    cmp("exit_reboot")
    {
        test_exit_reboot();
    }
    cmp("exit_unknown")
    {
        test_exit_unknown();
    }
    cmp("heartbeat_parse")
    {
        printf("Heartbeat parsing: %s\n", test_heartbeat_parsing() ? "PASS" : "FAIL");
    }
    cmp("circuit_breaker")
    {
        printf("Circuit-breaker: %s\n", test_circuit_breaker() ? "PASS" : "FAIL");
    }
    cmp("cooldown")
    {
        printf("Cooldown: %s\n", test_cooldown() ? "PASS" : "FAIL");
    }
    cmp("progress_timeout_config")
    {
        printf("Progress timeout config: %s\n", test_progress_timeout_config() ? "PASS" : "FAIL");
    }
    printf("Test finished\n");
}
