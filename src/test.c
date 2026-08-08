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
#include "process.h"

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

/* Parse of the a/o/r/s control commands (only heartbeat 'p' is covered elsewhere). */
int test_command_parsing()
{
    int pass = 1;
    struct { const char *in; net_command_type_t type; } cases[] =
    {
        { "aBot",  NET_CMD_START   },
        { "oBot",  NET_CMD_STOP    },
        { "rBot",  NET_CMD_RESTART },
        { "sBot",  NET_CMD_STATUS  },
        { "zzz",   NET_CMD_UNKNOWN },
    };

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        net_command_t cmd = cmd_parse_network(cases[i].in, (int)strlen(cases[i].in));

        if(cmd.type != cases[i].type)
        {
            printf("command parse: '%s' expected type %d, got %d\n",
                   cases[i].in, cases[i].type, cmd.type);
            pass = 0;
        }
    }

    // The app name must be the text after the command char.
    net_command_t named = cmd_parse_network("sBot", 4);
    if(strcmp(named.app_name, "Bot") != 0)
    {
        printf("command parse: expected app_name 'Bot', got '%s'\n", named.app_name);
        pass = 0;
    }

    return pass;
}

/* Exponential backoff: byte-identical to base_delay*2^(n-1) for small inputs, capped at
   3600s, and safe (no UB / negative) for large retry_count or base_delay. */
int test_backoff()
{
    int pass = 1;
    struct { int retry; int base; int want; } cases[] =
    {
        { 1, 1, 1    }, // 2^0
        { 2, 1, 2    }, // 2^1
        { 3, 2, 8    }, // 2 * 2^2
        { 5, 1, 16   }, // 2^4
        { 40, 1, 3600 }, // huge shift -> capped, no UB
        { 1, 100000, 3600 }, // huge base -> capped, no overflow
        { 0, 1, 1    }, // retry_count 0 -> shift clamped to 0
    };

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int got = calculate_backoff_delay(cases[i].retry, cases[i].base);

        if(got != cases[i].want)
        {
            printf("backoff(%d, %d) expected %d, got %d\n",
                   cases[i].retry, cases[i].base, cases[i].want, got);
            pass = 0;
        }
    }

    return pass;
}

/* parse_number: bounds, sign, and consumed-count on untrusted input. */
int test_parse_number()
{
    int pass = 1;
    int cnt;

    if(parse_number("12345", 5, &cnt) != 12345 || cnt != 5)
    {
        printf("parse_number: plain digits failed\n");
        pass = 0;
    }

    // Trailing non-digit stops the scan; cnt is the digits consumed.
    if(parse_number("42+", 3, &cnt) != 42 || cnt != 2)
    {
        printf("parse_number: trailing '+' handling failed (got cnt %d)\n", cnt);
        pass = 0;
    }

    // No digit -> 0, and the scan is bounded by length (no over-read).
    if(parse_number("xyz", 3, &cnt) != 0)
    {
        printf("parse_number: no-digit should return 0\n");
        pass = 0;
    }

    // length shorter than the digit run truncates.
    if(parse_number("999", 1, &cnt) != 9 || cnt != 1)
    {
        printf("parse_number: length bound failed\n");
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

/* Table of automated unit tests: each returns non-zero on pass. */
typedef int (*test_fn_t)(void);
static const struct { const char *name; test_fn_t fn; } unit_tests[] =
{
    { "heartbeat_parse",         test_heartbeat_parsing },
    { "command_parse",           test_command_parsing },
    { "backoff",                 test_backoff },
    { "parse_number",            test_parse_number },
    { "progress_timeout_config", test_progress_timeout_config },
};

/* Runs one unit test by name; prints PASS/FAIL and returns 0 on pass, 1 on fail. */
static int run_unit_test(const char *name)
{
    for(size_t i = 0; i < sizeof(unit_tests) / sizeof(unit_tests[0]); i++)
    {
        if(0 == strcmp(name, unit_tests[i].name))
        {
            int ok = unit_tests[i].fn() != 0;
            printf("%s: %s\n", unit_tests[i].name, ok ? "PASS" : "FAIL");
            return ok ? 0 : 1;
        }
    }

    return -1; // not a unit test
}

/*
    Runs a test by name and returns a process exit status: 0 = pass, non-zero = fail, so
    the caller (main) can propagate it to CI. "all" runs every automated unit test.
    The non-assertive demos (config/log/delay) and the exit_* supervisor fixtures return 0.
*/
int test(char *testname)
{
    if(0 == strlen(testname))
    {
        printf("Invalid testname!\n");
        return 1;
    }

    printf("\nTest: %s\n", testname);

    if(0 == strcmp(testname, "all"))
    {
        int failures = 0;

        for(size_t i = 0; i < sizeof(unit_tests) / sizeof(unit_tests[0]); i++)
        {
            failures += run_unit_test(unit_tests[i].name);
        }

        printf("Test finished: %d failure(s)\n", failures);
        return failures == 0 ? 0 : 1;
    }

    int unit = run_unit_test(testname);
    if(unit >= 0)
    {
        printf("Test finished\n");
        return unit;
    }

    // Non-assertive demos and supervisor fixtures (no pass/fail).
    cmp("filecmd")      { test_filecmd(); }
    cmp("config")       { test_config(); }
    cmp("log")          { test_log(); }
    cmp("delay")        { test_delay(); }
    cmp("exit_normal")  { test_exit_normal(); }
    cmp("exit_crash")   { test_exit_crash(); }
    cmp("exit_restart") { test_exit_restart(); }
    cmp("exit_reboot")  { test_exit_reboot(); }
    cmp("exit_unknown") { test_exit_unknown(); }

    printf("Test finished\n");
    return 0;
}
