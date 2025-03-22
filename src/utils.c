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
    int error = sysinfo(&s_info);

    if(error != 0)
    {
        fprintf(stderr, "code error = %d\n", error);
    }

    return s_info.uptime;
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

unsigned int find_replace_text(char *find, char *rep, char *buf, long size)
{
    long i;
    unsigned rpc = 0;
    size_t j, flen, rlen;
    flen = strlen(find);
    rlen = strlen(rep);

    for(i = 0; i < size; i++)
    {
        /* if char doesn't match first in find, continue */
        if(buf[i] != *find)
        {
            continue;
        }

        /* if find found, replace with rep */
        if(strncmp(&buf[i], find, flen) == 0)
        {
            for(j = 0; buf[i + j] && j < rlen; j++)
            {
                buf[i + j] = rep[j];
            }

            if(buf[i + j])
            {
                rpc++;
            }
        }
    }

    return rpc;
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

int parse_number(const char *ptr, int length, char *cnt)
{
    char minus = 0, i = 0;
    int sum = 0;

    /* Skip non-digit characters */
    while(!isdigit(*ptr) && *ptr != '-')
    {
        ptr++;
        i++;
    }

    /* Check for minus character */
    if(*ptr == '-')
    {
        minus = 1;
        ptr++;
        i++;
    }

    /* Parse number */
    while(isdigit(*ptr) && i < length)
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

void substring(char s[], char sub[], int p, int l)
{
    int c = 0;

    while(c < l)
    {
        sub[c] = s[p + c - 1];
        c++;
    }

    sub[c] = '\0';
}

char *timestamp(char *ts, int len)
{
    time_t date;
    time(&date);
    strftime(ts, len, "%Y-%m-%d %H:%M:%S", localtime(&date));
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
