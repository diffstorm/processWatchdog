/**
    @file test.c
    @brief Integration / runtime checks reachable via `-t <name>`.

    These run through the built binary end-to-end (real config parsing, the app array,
    status building, heartbeat logic) — the things an operator wants to verify on a box
    and that CI gates on via `-t all`. Pure-function unit tests (parsing, backoff, JSON
    field extraction) live in the GTest suite (test/server_test.cpp), not here.

    The exit_* entries are manual supervisor fixtures (they call exit() with a specific
    code); they are intentionally excluded from `-t all`.

    @author by Eray Ozturk | erayozturk1@gmail.com
    @url github.com/diffstorm
    @license GPL-3 License
*/

#include "apps.h"
#include "config.h"
#include "log.h"
#include "utils.h"
#include "cmd.h"
#include "process.h"
#include "status.h"
#include "heartbeat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* internal macros */
#define cmp(x)  if(0 == strcmp(testname, x))

// Writes a small multi-app INI to path; returns 0 on success.
static int write_sample_ini(const char *path)
{
    FILE *f = fopen(path, "w");
    if(!f)
    {
        return -1;
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
            "[app:Plain]\n"
            "start_delay = 5\n"
            "heartbeat_delay = 10\n"
            "heartbeat_interval = 15\n"
            "cmd = /bin/true\n");
    fclose(f);
    return 0;
}

/*
    Config parse (end-to-end): the real parser must load every app, preserve a configured
    progress_timeout (regression B1: it was reset to 0 after parse), and default it to 0 for
    apps that omit it.
*/
static int test_config_parse(void)
{
    const char *path = "test_pw_config.ini";
    if(write_sample_ini(path))
    {
        printf("config_parse: could not create temp ini\n");
        return 1;
    }

    Application_t apps[MAX_APPS];
    AppState_t state = {0};
    int rc = config_parse_file(path, apps, MAX_APPS, &state);
    remove(path);

    if(rc != 0)
    {
        printf("config_parse: parse failed (rc=%d)\n", rc);
        return 1;
    }

    int pass = 1;
    if(state.app_count != 2)
    {
        printf("config_parse: expected 2 apps, got %d\n", state.app_count);
        pass = 0;
    }

    for(int i = 0; i < state.app_count; i++)
    {
        if(0 == strcmp(apps[i].name, "WithProgress") && apps[i].progress_timeout != 30)
        {
            printf("config_parse: progress_timeout not preserved (got %d, want 30)\n",
                   apps[i].progress_timeout);
            pass = 0;
        }
        if(0 == strcmp(apps[i].name, "Plain") && apps[i].progress_timeout != 0)
        {
            printf("config_parse: omitted progress_timeout should default 0 (got %d)\n",
                   apps[i].progress_timeout);
            pass = 0;
        }
    }

    return pass ? 0 : 1;
}

/*
    Status pipeline (end-to-end): load config, build an app's status JSON, and read it back
    with the same field reader the client uses. Guards the operator-facing status contract.
*/
static int test_status_json(void)
{
    const char *path = "test_pw_status.ini";
    if(write_sample_ini(path))
    {
        printf("status_json: could not create temp ini\n");
        return 1;
    }

    Application_t apps[MAX_APPS];
    AppState_t state = {0};
    int rc = config_parse_file(path, apps, MAX_APPS, &state);
    remove(path);

    if(rc != 0 || state.app_count < 1)
    {
        printf("status_json: config load failed\n");
        return 1;
    }

    // build_status_json reads the live app array, so publish the parsed apps into it.
    memcpy(apps_get_array(), apps, sizeof(Application_t) * state.app_count);
    *apps_get_state() = state;

    char json[256];
    if(build_status_json(0, json, sizeof(json)) <= 0)
    {
        printf("status_json: build failed\n");
        return 1;
    }

    int pass = 1;
    char v[32];
    // A not-started app must report "stopped", and the app name must round-trip.
    if(status_extract_field(json, "app", v, sizeof(v)) != 0 || 0 != strcmp(v, "WithProgress"))
    {
        printf("status_json: app field wrong ('%s')\n", v);
        pass = 0;
    }
    if(status_extract_field(json, "status", v, sizeof(v)) != 0 || 0 != strcmp(v, "stopped"))
    {
        printf("status_json: status should be 'stopped' for a not-started app (got '%s')\n", v);
        pass = 0;
    }

    return pass ? 0 : 1;
}

/*
    Heartbeat timeout (end-to-end): with a fresh heartbeat the app is healthy; once the
    interval elapses (simulated by ageing last_heartbeat) it must be reported as timed out.
*/
static int test_heartbeat_timeout(void)
{
    Application_t *app = &apps_get_array()[0];
    memset(app, 0, sizeof(*app));
    app->started = true;
    app->heartbeat_interval = 10;
    app->first_heartbeat = true; // use the regular interval, not the first-heartbeat grace

    int pass = 1;

    heartbeat_update_time(0);            // fresh heartbeat now
    if(heartbeat_is_timeout(0))
    {
        printf("heartbeat_timeout: fresh heartbeat wrongly reported as timed out\n");
        pass = 0;
    }

    // Age the last heartbeat well past the interval.
    app->last_heartbeat = (time_t)(get_monotonic_time() - (app->heartbeat_interval + 5));
    if(!heartbeat_is_timeout(0))
    {
        printf("heartbeat_timeout: aged heartbeat not reported as timed out\n");
        pass = 0;
    }

    return pass ? 0 : 1;
}

// --- Supervisor fixtures: exit with a specific code (manual use; not part of `-t all`) ---

static void test_exit_normal(void)  { printf("Exit normal\n");  exit(EXIT_NORMALLY); }
static void test_exit_crash(void)   { printf("Exit crash\n");   exit(EXIT_CRASHED); }
static void test_exit_restart(void) { printf("Exit restart\n"); exit(EXIT_RESTART); }
static void test_exit_reboot(void)  { printf("Exit reboot\n");  exit(EXIT_REBOOT); }
static void test_exit_unknown(void) { printf("Exit unknown\n"); exit(123); }

/* Table of automated integration tests: each returns 0 on pass. Run individually or via "all". */
typedef int (*test_fn_t)(void);
static const struct { const char *name; test_fn_t fn; } integration_tests[] =
{
    { "config_parse",      test_config_parse },
    { "status_json",       test_status_json },
    { "heartbeat_timeout", test_heartbeat_timeout },
};

// Runs one integration test by name; prints PASS/FAIL. Returns 0 pass / 1 fail / -1 not found.
static int run_integration_test(const char *name)
{
    for(size_t i = 0; i < sizeof(integration_tests) / sizeof(integration_tests[0]); i++)
    {
        if(0 == strcmp(name, integration_tests[i].name))
        {
            int rc = integration_tests[i].fn();
            printf("%s: %s\n", integration_tests[i].name, rc == 0 ? "PASS" : "FAIL");
            return rc == 0 ? 0 : 1;
        }
    }

    return -1;
}

/*
    Runs a test by name and returns a process exit status: 0 = pass, non-zero = fail, so the
    caller (main) can propagate it to CI. "all" runs every automated integration test.
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

        for(size_t i = 0; i < sizeof(integration_tests) / sizeof(integration_tests[0]); i++)
        {
            failures += run_integration_test(integration_tests[i].name);
        }

        printf("Test finished: %d failure(s)\n", failures);
        return failures == 0 ? 0 : 1;
    }

    int rc = run_integration_test(testname);
    if(rc >= 0)
    {
        printf("Test finished\n");
        return rc;
    }

    // Supervisor fixtures (exit with a specific code; not part of `-t all`).
    cmp("exit_normal")  { test_exit_normal(); }
    cmp("exit_crash")   { test_exit_crash(); }
    cmp("exit_restart") { test_exit_restart(); }
    cmp("exit_reboot")  { test_exit_reboot(); }
    cmp("exit_unknown") { test_exit_unknown(); }

    printf("Unknown test: %s\n", testname);
    return 1;
}
