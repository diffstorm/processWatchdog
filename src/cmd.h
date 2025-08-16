/**
    @file cmd.h
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

#ifndef CMD_H
#define CMD_H

#include <stdbool.h>

// Include apps.h for MAX_APP_NAME_LENGTH constant
#include "apps.h"

/**
    @enum net_command_type_t
    @brief Enumeration of network command types.
*/
typedef enum {
    NET_CMD_HEARTBEAT,    /**< Heartbeat command: p<pid> */
    NET_CMD_START,        /**< Start command: a<name> (future) */
    NET_CMD_STOP,         /**< Stop command: o<name> (future) */
    NET_CMD_RESTART,      /**< Restart command: r<name> (future) */
    NET_CMD_UNKNOWN       /**< Unknown or invalid command */
} net_command_type_t;

/**
    @struct net_command_t
    @brief Structure representing a parsed network command.
*/
typedef struct {
    net_command_type_t type;                    /**< Type of the command */
    int pid;                                    /**< Process ID for heartbeat commands */
    char app_name[MAX_APP_NAME_LENGTH];         /**< Application name for control commands */
} net_command_t;

/**
    @brief Parses network command data and returns a structured command.

    This function parses raw command data received over network protocols
    and returns a structured representation of the command.

    @param data Pointer to the command data.
    @param length Length of the command data.
    @return Parsed network command structure.
*/
net_command_t cmd_parse_network(const char* data, int length);

#endif // CMD_H