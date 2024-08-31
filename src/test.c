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
#include "server.h"
#include "filecmd.h"
#include "log.h"
#include "utils.h"

/* internal macros */
#define cmp(x)  if(0 == strcmp(testname, x))
#define chk(x)  printf("%s\n", false != x() ? "Success" : "Fail!");

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
    // TODO: add other unit tests
    printf("Test finished\n");
}
