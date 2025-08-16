/**
    @file cmd.c
    @brief Network Command Parsing Module

    This module handles parsing of commands received over network protocols
    (UDP/TCP) for the Process Watchdog application. It provides functions to
    parse and identify different types of network commands.

    @date 2023-01-01
    @version 1.0
    @author by Eray Ozturk | erayozturk1@gmail.com
    @url github.com/diffstorm
    @license GPL-3 License
*/

#include "cmd.h"
#include "log.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

net_command_t cmd_parse_network(const char *data, int length)
{
    net_command_t cmd = {0};
    cmd.type = NET_CMD_UNKNOWN;
    cmd.pid = 0;
    cmd.app_name[0] = '\0';

    if(!data || length <= 0)
    {
        return cmd;
    }

    switch(data[0])
    {
        case 'p': // pid heartbeat : p<pid> ? p1234
        {
            int n = parse_number(data, length, NULL);
            LOGD("Heartbeat command received from pid %d : %s", n, data);

            if(0 < n && INT32_MAX > n)
            {
                cmd.type = NET_CMD_HEARTBEAT;
                cmd.pid = n;
            }
            else
            {
                LOGE("Invalid pid received, pid %d : %s", n, data);
                cmd.type = NET_CMD_UNKNOWN;
            }
        }
        break;

        case 'a': // stArt : a<name> ? aBot (currently disabled)
        {
            LOGD("Start command received: %s", data);
            cmd.type = NET_CMD_START;
            strncpy(cmd.app_name, &data[1], MAX_APP_NAME_LENGTH - 1);
            cmd.app_name[MAX_APP_NAME_LENGTH - 1] = '\0';
        }
        break;

        case 'o': // stOp : o<name> ? oBot (currently disabled)
        {
            LOGD("Stop command received: %s", data);
            cmd.type = NET_CMD_STOP;
            strncpy(cmd.app_name, &data[1], MAX_APP_NAME_LENGTH - 1);
            cmd.app_name[MAX_APP_NAME_LENGTH - 1] = '\0';
        }
        break;

        case 'r': // Restart : r<name> ? rBot (currently disabled)
        {
            LOGD("Restart command received: %s", data);
            cmd.type = NET_CMD_RESTART;
            strncpy(cmd.app_name, &data[1], MAX_APP_NAME_LENGTH - 1);
            cmd.app_name[MAX_APP_NAME_LENGTH - 1] = '\0';
        }
        break;

        default:
        {
            const int max_bytes = MAX_APP_NAME_LENGTH;
            const int max_length = max_bytes * 3;
            char hexStr[max_length];
            memset(hexStr, 0, sizeof(hexStr));
            int safe_length = length;

            if(safe_length > max_bytes)
            {
                safe_length = max_bytes;
            }

            for(int i = 0; i < safe_length; i++)
            {
                snprintf(&hexStr[i * 3], sizeof(hexStr) - (i * 3), "%02X ", data[i]);
            }

            char printableStr[max_bytes + 1];
            memset(printableStr, 0, sizeof(printableStr));

            for(int i = 0; i < safe_length; i++)
            {
                printableStr[i] = (data[i] >= 32 && data[i] < 127) ? data[i] : '.';
            }

            LOGE("Unknown command received : %s | %s", printableStr, hexStr);
            cmd.type = NET_CMD_UNKNOWN;
        }
        break;
    }

    return cmd;
}
