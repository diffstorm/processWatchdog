/**
    @file utils.c
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

#include "utils.h"

char *pstrcpy(char *s1, const char *s2)
{
    char *dst = s1;
    const char *src = s2;
    unsigned int len = 0;

    while((*dst++ = *src++) != '\0')
    {
        len++;
    }

    return s1 + len;
}

long get_uptime()
{
    struct sysinfo s_info;

    if(sysinfo(&s_info) != 0)
    {
        // On failure s_info is uninitialized; returning s_info.uptime would be UB.
        fprintf(stderr, "sysinfo failed: %s\n", strerror(errno));
        return 0;
    }

    return s_info.uptime;
}

/**
    @brief Gets the monotonic time in seconds.
    @return Monotonic time in seconds.
*/
long get_monotonic_time(void)
{
    struct timespec ts;

    if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        // On failure ts is uninitialized; return 0 (same convention as time_ms()). Elapsed
        // computations (now - last) then fail safe toward "not yet timed out".
        fprintf(stderr, "clock_gettime(CLOCK_MONOTONIC) failed: %s\n", strerror(errno));
        return 0;
    }

    return ts.tv_sec;
}

/**
    @brief Gets the process start time from /proc/<pid>/stat in clock ticks since boot.

    According to proc(5), field 22 in /proc/<pid>/stat is the process start time
    in clock ticks since system boot.

    @param pid Process ID to query.
    @return Process start time in clock ticks since boot, or 0 on error.
*/
unsigned long get_process_start_time(pid_t pid)
{
    char path[256];
    char buffer[1024];
    FILE *fp;
    size_t bytes_read;
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");

    if(fp == NULL)
    {
        return 0;
    }

    bytes_read = fread(buffer, 1, sizeof(buffer) - 1, fp);

    if(bytes_read == 0)
    {
        fclose(fp);
        return 0;
    }

    fclose(fp);
    buffer[bytes_read] = '\0';
    // Find the last ')' in the command name (field 2 is command in parentheses)
    // Then find field 22 (start time)
    char *ptr = strrchr(buffer, ')');

    if(ptr == NULL)
    {
        return 0;
    }

    // Field 3 begins after the command. Advance through fields 3-21 so ptr
    // points at field 22, the start time.
    ptr++;

    while(*ptr == ' ')
    {
        ptr++;
    }

    int field = 3;

    while(field < 22 && *ptr != '\0')
    {
        while(*ptr == ' ')
        {
            ptr++;
        }

        while(*ptr != ' ' && *ptr != '\0')
        {
            ptr++;
        }

        field++;
    }

    if(field != 22 || *ptr == '\0')
    {
        return 0;
    }

    errno = 0;
    char *end = NULL;
    unsigned long start_time = strtoul(ptr, &end, 10);

    if(errno != 0 || end == ptr)
    {
        return 0;
    }

    return start_time;
}

void delay_ms(int ms)
{
    if(ms > 1000)
    {
        sleep(ms / 1000);
    }
    else
    {
        usleep(ms * 1000);
    }
}

void delay(int sec)
{
    delay_ms(sec * 1000);
}

clk_t time_ms(void)
{
    struct timespec now;

    if(clock_gettime(CLOCK_MONOTONIC, &now))
    {
        return 0;
    }

    return (clk_t)((now.tv_sec * 1000) + (now.tv_nsec / 1e6));
}

clk_t elapsed_ms(clk_t clk)
{
    clk_t c = time_ms();
    return (c - clk);
}

void run_command(char *command)
{
    char *token;
    char *argv[1024] = {NULL};
    int i = 0;
    token = strtok(command, " ");

    while(token != NULL)
    {
        argv[i++] = token;
        token = strtok(NULL, " ");
    }

    execv(argv[0], argv);
    perror("execv");
}

void f_rename(const char *fromfilename, const char *tofilename)
{
    int ret = rename(fromfilename, tofilename);

    if(ret != 0)
    {
        fprintf(stderr, "Cannot rename file %s: %s\n",
                fromfilename, strerror(errno));
    }
}

bool f_exist(const char *filename)
{
    struct stat st;
    return (stat(filename, &st) == 0);
}

int f_size(const char *filename)
{
    struct stat st;

    if(stat(filename, &st) == 0)
    {
        return (int)st.st_size;
    }

    fprintf(stderr, "Cannot determine size of %s: %s\n",
            filename, strerror(errno));
    return -1;
}

int f_read(const char *filename, char *buf, size_t size)
{
    size_t len = 0;
    FILE *fp = fopen(filename, "r");

    if(fp != NULL)
    {
        len = fread(buf, sizeof(char), size, fp);

        if(ferror(fp) != 0 || len != size)
        {
            fprintf(stderr, "Error reading file %s: %s\n",
                    filename, strerror(errno));
        }
        else
        {
            buf[len++] = '\0'; /* Just to be safe. */
        }

        fclose(fp);
    }

    return (int)len;
}

int f_write(const char *filename, char *buf, size_t size)
{
    size_t len = 0;
    FILE *fp = fopen(filename, "w");

    if(fp != NULL)
    {
        len = fwrite(buf, sizeof(char), size, fp);

        if(ferror(fp) != 0 || len != size)
        {
            fprintf(stderr, "Error writing into file %s: %s\n",
                    filename, strerror(errno));
        }

        fclose(fp);
    }

    return (int)len;
}

void f_create(const char *filename)
{
    FILE *fp = fopen(filename, "w");

    if(fp != NULL)
    {
        if(ferror(fp) != 0)
        {
            fprintf(stderr, "Error creating file %s: %s\n",
                    filename, strerror(errno));
        }

        fclose(fp);
    }
}

void f_remove(const char *filename)
{
    if(remove(filename) != 0)
    {
        fprintf(stderr, "File %s not deleted: %s\n",
                filename, strerror(errno));
    }
}

char *sgets(char *s, int n, const char **strp)
{
    if(**strp == '\0')
    {
        return NULL;
    }

    int i;

    for(i = 0; i < n - 1; ++i, ++(*strp))
    {
        s[i] = **strp;

        if(**strp == '\0')
        {
            break;
        }

        if(**strp == '\n')
        {
            s[i + 1] = '\0';
            ++(*strp);
            break;
        }
    }

    if(i == n - 1)
    {
        s[i] = '\0';
    }

    return s;
}

unsigned short crc16(const unsigned char *data_p, unsigned char length)
{
    unsigned char x;
    unsigned short crc = 0xFFFF;

    while(length--)
    {
        x = crc >> 8 ^ *data_p++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((unsigned short)(x << 12)) ^ ((unsigned short)(x << 5)) ^ ((unsigned short)x);
    }

    return crc;
}

void *findin(void *haystack, size_t haystacksize, void *needle, size_t needlesize)
{
    unsigned char *hptr = (unsigned char *)haystack;
    unsigned char *nptr = (unsigned char *)needle;
    unsigned int i;

    /* Check sizes */
    if(needlesize > haystacksize)
    {
        /* Needle is greater than haystack = nothing in memory */
        return 0;
    }

    /* Check if same length */
    if(haystacksize == needlesize)
    {
        if(memcmp(hptr, nptr, needlesize) == 0)
        {
            return hptr;
        }

        return 0;
    }

    /* Set haystack size pointers */
    haystacksize -= needlesize;

    /* Go through entire memory */
    for(i = 0; i < haystacksize; i++)
    {
        /* Check memory match */
        if(memcmp(&hptr[i], nptr, needlesize) == 0)
        {
            return &hptr[i];
        }
    }

    return 0;
}

int parse_number(const char *ptr, int length, int *cnt)
{
    char minus = 0;
    int i = 0;
    int sum = 0;

    /* Skip leading non-digit characters. Bounded by length and stopped at NUL so a datagram
       with no digit cannot walk past the buffer (remote OOB read); cast to unsigned char
       because isdigit() on a negative (high-bit) char is undefined. */
    while(i < length && *ptr != '\0' && !isdigit((unsigned char)*ptr) && *ptr != '-')
    {
        ptr++;
        i++;
    }

    /* Check for minus character */
    if(i < length && *ptr == '-')
    {
        minus = 1;
        ptr++;
        i++;
    }

    /* Parse number (length checked before the deref; unsigned-char cast for isdigit) */
    while(i < length && isdigit((unsigned char)*ptr))
    {
        sum = 10 * sum + ((*ptr) - '0');
        ptr++;
        i++;
    }

    /* Save number of characters used for number */
    if(cnt != NULL)
    {
        *cnt = i;
    }

    /* Minus detected */
    if(minus)
    {
        return 0 - sum;
    }

    /* Return number */
    return sum;
}

bool parse_int(const char *ptr, int min_val, int max_val, int *result)
{
    char *endptr;
    errno = 0;
    long tmp = strtol(ptr, &endptr, 10);

    if(errno != 0 || *endptr != '\0' || tmp < min_val || tmp > max_val)
    {
        return false;
    }

    *result = (int)tmp;
    return true;
}

char *timestamp(char *ts, int len)
{
    time_t date;
    time(&date);
    struct tm tm_buf;

    // localtime_r (reentrant, as strtok_r is already used in this project) into a local tm;
    // guard against NULL so a bad time_t never reaches strftime.
    if(localtime_r(&date, &tm_buf) != NULL)
    {
        strftime(ts, len, "%Y-%m-%d %H:%M:%S", &tm_buf);
    }
    else if(len > 0)
    {
        ts[0] = '\0';
    }

    return ts;
}

char *humansize(uint64_t bytes, char *sz, int len)
{
    char *suffix[] = {"B", "KB", "MB", "GB", "TB"};
    char length = sizeof(suffix) / sizeof(suffix[0]);
    int i = 0;
    double dblBytes = bytes;

    if(bytes > 1024)
    {
        for(i = 0; (bytes / 1024) > 0 && i < length - 1; i++, bytes /= 1024)
        {
            dblBytes = bytes / 1024.0;
        }
    }

    snprintf(sz, len, "%.02lf %s", dblBytes, suffix[i]);
    return sz;
}

void toLower(char *s)
{
    for(; *s; s++)
    {
        *s = tolower(*s);
    }
}
