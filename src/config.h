/**
    @file config.h
    @brief Configuration Management Module

    This module handles INI file parsing, validation, and configuration management
    for the Process Watchdog application. It provides functions to load, validate,
    and monitor configuration files.

    @date 2023-01-01
    @version 1.0
    @author by Eray Ozturk | erayozturk1@gmail.com
    @url github.com/diffstorm
    @license GPL-3 License
*/

#ifndef CONFIG_H
#define CONFIG_H

#include "apps.h"

#include <stdbool.h>
#include <time.h>

/**
    @brief Checks if the INI file has been modified since the last check.

    @param ini_path Path to the INI file.
    @param last_modified Last known modification time.
    @return true if the INI file has been modified, false otherwise.
*/
bool config_is_file_updated(const char *ini_path, time_t last_modified);

/**
    @brief Validates the INI file path and accessibility.

    @param ini_path Path to the INI file to validate.
    @return 0 if valid, 1 otherwise.
*/
int config_validate_file(const char *ini_path);

/**
    @brief Parses the INI configuration file and populates application data.

    This function reads and parses an INI configuration file, populating the provided
    application array and state structure with the configuration data.

    @param ini_path Path to the INI file.
    @param apps Array of Application_t structures to populate.
    @param max_apps Maximum number of applications that can be stored.
    @param state AppState_t structure to populate with global configuration.
    @return 0 if successful, 1 otherwise.
*/
int config_parse_file(const char *ini_path, Application_t *apps, int max_apps, AppState_t *state);

#endif // CONFIG_H
