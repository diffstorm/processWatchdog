/**
    @file status.h
    @brief Process status reporting: JSON contract (server side) + a small UDP query client.

    The status JSON is the wire contract between the running watchdog (which answers the
    `s<app>` UDP command) and any client. build_status_json() is used by both the server
    handler and is unit-tested directly, so the format has a single source of truth.

    @author by Eray Ozturk | erayozturk1@gmail.com
    @url github.com/diffstorm
    @license GPL-3 License
*/

#ifndef STATUS_H
#define STATUS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
    @brief Builds the status JSON for one application into buf.

    Format: {"app":"<name>","status":"...","circuit":"...","cooldown":N,"failures":N,"ack_required":true|false}

    @param app_index Index into the applications array.
    @param buf Output buffer.
    @param size Size of buf.
    @return Number of characters written (excluding NUL), or -1 on error.
*/
int build_status_json(int app_index, char *buf, size_t size);

/**
    @brief Extracts a string/number field value from a status JSON object.

    Minimal, dependency-free reader for the fixed shape produced by build_status_json
    (not a general JSON parser). Copies the value for "key" (without surrounding quotes
    for strings) into out.

    @param json The JSON text.
    @param key The field name to read (e.g. "status").
    @param out Output buffer for the value.
    @param size Size of out.
    @return 0 on success, -1 if the key was not found.
*/
int status_extract_field(const char *json, const char *key, char *out, size_t size);

/**
    @brief Client mode: query the running watchdog over UDP and print the result.

    Sends the status command for one app (or every app in the config when app is NULL/empty)
    to 127.0.0.1:port, then prints either an aligned human table or the raw JSON lines.
    Reads the app list from the already-loaded config. Intended to run and exit, not as part
    of the daemon loop.

    @param app App name, or NULL/"" for all apps in the config.
    @param port UDP port the watchdog listens on.
    @param json_mode Non-zero to print raw JSON lines instead of a table.
    @return 0 if at least one app responded, non-zero otherwise.
*/
int status_client(const char *app, int port, int json_mode);

#ifdef __cplusplus
}
#endif

#endif // STATUS_H
