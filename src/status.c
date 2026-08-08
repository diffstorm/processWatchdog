/**
    @file status.c
    @brief Process status reporting: JSON contract + UDP query client. See status.h.

    @author by Eray Ozturk | erayozturk1@gmail.com
    @url github.com/diffstorm
    @license GPL-3 License
*/

#include "status.h"
#include "apps.h"
#include "process.h"
#include "server.h"
#include "stats.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

int build_status_json(int app_index, char *buf, size_t size)
{
    if(!buf || size == 0 || app_index < 0 || app_index >= get_app_count())
    {
        return -1;
    }

    Application_t *app = &apps_get_array()[app_index];

    // Process state
    const char *status;
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

    // Circuit-breaker state ("disabled" when the feature is not configured)
    const char *circuit;
    if(app->circuit_breaker_threshold > 0)
    {
        if(app->circuit_state == 1)
        {
            circuit = "open";
        }
        else if(app->circuit_state == 2)
        {
            circuit = "half_open";
        }
        else
        {
            circuit = "closed";
        }
    }
    else
    {
        circuit = "disabled";
    }

    bool ack_required = app->in_cooldown && app->cooldown_manual_ack_threshold > 0 &&
                        app->cooldown_count >= app->cooldown_manual_ack_threshold;

    // cooldown_end_time is monotonic (process.c), so compare against the monotonic clock.
    int cooldown_remaining = 0;
    if(app->in_cooldown)
    {
        long now = get_monotonic_time();
        if(now < app->cooldown_end_time)
        {
            cooldown_remaining = (int)(app->cooldown_end_time - now);
        }
    }

    // Report live resource use only while running; a stopped app reports pid 0 and 0 usage
    // rather than the last frozen sample.
    int pid = get_app_pid(app_index);
    float cpu = 0.0f;
    size_t mem_kb = 0;
    if(process_is_running(app_index))
    {
        cpu = stats_get_cpu_percent(app_index);
        mem_kb = stats_get_memory_kb(app_index);
    }

    int n = snprintf(buf, size,
                     "{\"app\":\"%s\",\"status\":\"%s\",\"circuit\":\"%s\",\"pid\":%d,"
                     "\"cpu\":%.2f,\"mem_kb\":%zu,\"cooldown\":%d,\"failures\":%d,\"ack_required\":%s}",
                     app->name, status, circuit, pid, cpu, mem_kb,
                     cooldown_remaining, app->failure_count, ack_required ? "true" : "false");

    if(n < 0 || (size_t)n >= size)
    {
        return -1;    // truncated or encoding error
    }

    return n;
}

int status_extract_field(const char *json, const char *key, char *out, size_t size)
{
    if(!json || !key || !out || size == 0)
    {
        return -1;
    }

    // Look for "key":
    char needle[64];
    int nn = snprintf(needle, sizeof(needle), "\"%s\":", key);
    if(nn < 0 || (size_t)nn >= sizeof(needle))
    {
        return -1;
    }

    const char *p = strstr(json, needle);
    if(!p)
    {
        return -1;
    }

    p += nn; // move past "key":

    // String value: "..." — copy without the quotes. Otherwise copy the bare token
    // up to the next ',' or '}'.
    size_t i = 0;
    if(*p == '"')
    {
        p++;
        while(*p && *p != '"' && i + 1 < size)
        {
            out[i++] = *p++;
        }
    }
    else
    {
        while(*p && *p != ',' && *p != '}' && i + 1 < size)
        {
            out[i++] = *p++;
        }
    }

    out[i] = '\0';
    return 0;
}

// Sends `msg` to 127.0.0.1:port on a fresh socket and waits briefly for one reply.
// Returns the reply length (reply NUL-terminated), or <= 0 on send failure / no reply.
static int status_query_one(const char *msg, int port, char *reply, int reply_size)
{
    int sock;
    if(udp_start(&sock, 0)) // ephemeral local port
    {
        return -1;
    }

    int rc = -1;
    if(udp_send(sock, msg, (int)strlen(msg), "127.0.0.1", port) == 0)
    {
        int len = reply_size - 1;
        // 500 ms is ample for a local reply; a missed reply just prints as no-response.
        if(udp_poll(sock, 500, reply, &len) == 0 && len > 0)
        {
            reply[len] = '\0';
            rc = len;
        }
    }

    udp_stop(sock);
    return rc;
}

#define STATUS_HEADER_FMT "%-20s %-13s %-10s %7s %6s %10s %8s %6s %5s\n"

static void print_status_header(void)
{
    printf(STATUS_HEADER_FMT, "APP", "STATUS", "CIRCUIT", "PID", "CPU%", "MEM(KB)", "COOLDOWN", "FAILS", "ACK");
}

// Prints one JSON reply as an aligned table row (dashes for any missing field).
static void print_status_row(const char *name, const char *json)
{
    char status[32] = "-", circuit[32] = "-", pid[12] = "-", cpu[12] = "-",
         mem[16] = "-", cooldown[12] = "-", failures[12] = "-", ack[8] = "-";
    status_extract_field(json, "status", status, sizeof(status));
    status_extract_field(json, "circuit", circuit, sizeof(circuit));
    status_extract_field(json, "pid", pid, sizeof(pid));
    status_extract_field(json, "cpu", cpu, sizeof(cpu));
    status_extract_field(json, "mem_kb", mem, sizeof(mem));
    status_extract_field(json, "cooldown", cooldown, sizeof(cooldown));
    status_extract_field(json, "failures", failures, sizeof(failures));
    status_extract_field(json, "ack_required", ack, sizeof(ack));
    printf(STATUS_HEADER_FMT, name, status, circuit, pid, cpu, mem, cooldown, failures, ack);
}

// Queries one app and prints its result. Prints the table header lazily on the first
// response (so a fully-unresponsive watchdog shows a clear message, not an empty table).
// Returns 1 if the app responded, 0 otherwise. *header_printed tracks the lazy header.
static int status_query_and_print(const char *name, int port, int json_mode, bool *header_printed)
{
    char msg[MAX_APP_NAME_LENGTH + 1];
    snprintf(msg, sizeof(msg), "s%s", name);

    char reply[256];
    if(status_query_one(msg, port, reply, sizeof(reply)) <= 0)
    {
        return 0;
    }

    if(json_mode)
    {
        printf("%s\n", reply);
    }
    else
    {
        if(!*header_printed)
        {
            print_status_header();
            *header_printed = true;
        }
        print_status_row(name, reply);
    }

    return 1;
}

int status_client(const char *app, int port, int json_mode)
{
    bool all = (app == NULL || app[0] == '\0' || strcmp(app, "*") == 0);
    bool header_printed = false;
    int responded = 0;

    if(all)
    {
        int count = get_app_count();
        if(count <= 0)
        {
            fprintf(stderr, "No apps configured.\n");
            return 1;
        }

        for(int i = 0; i < count; i++)
        {
            responded += status_query_and_print(get_app_name(i), port, json_mode, &header_printed);
        }

        if(responded == 0)
        {
            fprintf(stderr, "No apps responded (is the watchdog running on port %d?)\n", port);
        }
    }
    else
    {
        responded = status_query_and_print(app, port, json_mode, &header_printed);
        if(responded == 0)
        {
            fprintf(stderr, "No response for '%s' (is the watchdog running on port %d?)\n", app, port);
        }
    }

    return responded > 0 ? 0 : 1;
}
