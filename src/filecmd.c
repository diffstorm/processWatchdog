/**
    @file filecmd.c
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
#include "filecmd.h"
#include "log.h"
#include "utils.h"

typedef enum
{
    fc_START = 0,
    fc_STOP,
    fc_RESTART,
    fc_END
} action_t;

static const char *prefixes[] =
{
    "start",
    "stop",
    "restart",
    0
};

static void get_file_name(action_t action, int i, char *out)
{
    char *p;
    p = out;
    p = pstrcpy(p, prefixes[action]);
    strncpy(p, get_app_name(i), MAX_APP_NAME_LENGTH);
    toLower(out);
}

static bool is_file_exist(action_t action, int i)
{
    char fname[MAX_APP_NAME_LENGTH * 2];
    get_file_name(action, i, fname);
    return f_exist(fname);
}

static void remove_file(action_t action, int i)
{
    char fname[MAX_APP_NAME_LENGTH * 2];
    get_file_name(action, i, fname);
    f_remove(fname);
}

static void create_file(action_t action, int i)
{
    char fname[MAX_APP_NAME_LENGTH * 2];
    get_file_name(action, i, fname);
    f_create(fname);
}

bool filecmd_start(int i)
{
    return is_file_exist(fc_START, i);
}

bool filecmd_stop(int i)
{
    return is_file_exist(fc_STOP, i);
}

bool filecmd_restart(int i)
{
    return is_file_exist(fc_RESTART, i);
}

void filecmd_remove_start(int i)
{
    if(filecmd_start(i))
    {
        remove_file(fc_START, i);
    }
}

void filecmd_remove_stop(int i)
{
    if(filecmd_stop(i))
    {
        remove_file(fc_STOP, i);
    }
}

void filecmd_remove_restart(int i)
{
    if(filecmd_restart(i))
    {
        remove_file(fc_RESTART, i);
    }
}

void filecmd_create_start(int i)
{
    if(!filecmd_start(i))
    {
        create_file(fc_START, i);
    }
}

void filecmd_create_stop(int i)
{
    if(!filecmd_stop(i))
    {
        create_file(fc_STOP, i);
    }
}

void filecmd_create_restart(int i)
{
    if(!filecmd_restart(i))
    {
        create_file(fc_RESTART, i);
    }
}

bool filecmd_exists(const char *fname)
{
    bool ret = f_exist(fname);

    if(ret)
    {
        f_remove(fname);
    }

    return ret;
}
