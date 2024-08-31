/**
    @file utils.h
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

#ifndef UTILS_H
#define UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h> // PRIu64
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/sysinfo.h>

#if __STDC_VERSION__ >= 199901L
#define C99
#endif

#ifdef C99
#include <stdbool.h>
#elif __cplusplus
#else
typedef enum
{
    false = 0,
    true = 1
} bool;
#endif

typedef uint64_t clk_t;

/**
    @file utils.h
    @brief Utility functions for common operations.
*/

/* app exit codes */
#define EXIT_NORMALLY   0 /**< Exit code for normal termination. */
#define EXIT_CRASHED    1 /**< Exit code for termination due to a crash. */
#define EXIT_RESTART    2 /**< Exit code for restarting the application. */
#define EXIT_REBOOT     3 /**< Exit code for rebooting the system. */

/* macros */
#define UNUSED(x) (void)(x) /**< Macro to suppress compiler warnings about unused variables. */

/* functions */

/**
    @brief Copies a string with pointer of end position.

    @param s1 Destination string.
    @param s2 Source string.
    @return Pointer to the destination string.
*/
char *pstrcpy(char *s1, const char *s2);

/**
    @brief Gets the system uptime in seconds.

    @return System uptime in seconds.
*/
long get_uptime();

/**
    @brief Waits for a specified number of milliseconds.

    @param ms Number of milliseconds to wait.
*/
void delay_ms(int ms);

/**
    @brief Waits for a specified number of seconds.

    @param sec Number of seconds to wait.
*/
void delay(int sec);

/**
    @brief Initializes a clock for measuring time.

    This can be used with elapsed_ms() for measuring a duration.
    This function initializes a MONOTONIC clock for time measurement.
    For *nix operating systems, this initializes a MONOTONIC clock.

    @return The clk_t initialized to the current time.
*/
clk_t time_ms(void);

/**
    @brief Return the elapsed time in milliseconds since time_ms() was last called.

    @param clk The clock initialized by time_ms().
    @return The number of milliseconds elapsed since time_ms() was last called.
*/
clk_t elapsed_ms(clk_t clk);

/**
    @brief Executes a shell command using execv.

    @param command The shell command to execute.
*/
void run_command(char *command);

/**
    @brief Renames a file.

    @param fromfilename Current filename.
    @param tofilename New filename.
*/
void f_rename(const char *fromfilename, const char *tofilename);

/**
    @brief Checks if a file exists.

    @param filename Name of the file.
    @return true if the file exists, false otherwise.
*/
bool f_exist(const char *filename);

/**
    @brief Gets the size of a file.

    @param filename Name of the file.
    @return Size of the file in bytes.
*/
int f_size(const char *filename);

/**
    @brief Reads data from a file.

    @param filename Name of the file.
    @param buf Buffer to store the read data.
    @param size Size of the buffer.
    @return Number of bytes read.
*/
int f_read(const char *filename, char *buf, size_t size);

/**
    @brief Writes data to a file.

    @param filename Name of the file.
    @param buf Buffer containing the data to write.
    @param size Size of the data to write.
    @return Number of bytes written.
*/
int f_write(const char *filename, char *buf, size_t size);

/**
    @brief Creates a file.

    @param filename Name of the file to create.
*/
void f_create(const char *filename);

/**
    @brief Removes a file.

    @param filename Name of the file to remove.
*/
void f_remove(const char *filename);

/**
    @brief Finds and replaces text in a buffer.

    @param find Text to find.
    @param rep Text to replace with.
    @param buf Buffer containing the text.
    @param size Size of the buffer.
    @return Number of replacements made.
*/
unsigned int find_replace_text(char *find, char *rep, char *buf, long size);

/**
    @brief Reads a string from a memory location.

    This function is similar to fgets() but reads from a memory location instead of a file.

    @param s Pointer to the destination buffer.
    @param n Maximum number of characters to read.
    @param strp Pointer to the memory location to read from.
    @return Pointer to the destination buffer.

    @example
    int main(){
        const char *data = "abc\nefg\nhhh\nij";
        char buff[16];
        const char **p = &data;

        while(NULL!=sgets(buff, sizeof(buff), p))
            printf("%s", buff);
        return 0;
    }
*/
char *sgets(char *s, int n, const char **strp);

/**
    @brief Calculates the CRC16 checksum of data.

    @param data_p Pointer to the data.
    @param length Length of the data.
    @return CRC16 checksum.
*/
unsigned short crc16(const unsigned char *data_p, unsigned char length);

/**
    @brief Finds a needle in a haystack.

    @param haystack Pointer to the haystack.
    @param haystacksize Size of the haystack.
    @param needle Pointer to the needle.
    @param needlesize Size of the needle.
    @return Pointer to the found needle in the haystack, or NULL if not found.
*/
void *findin(void *haystack, size_t haystacksize, void *needle, size_t needlesize);

/**
    @brief Parses and returns a number from a string.

    @param ptr Pointer to the string.
    @param length Length of the string.
    @param cnt Pointer to store the parsed number.
    @return 0 on success, -1 on failure.
*/
int parse_number(const char *ptr, int length, char *cnt);

/**
    @brief Copies in string in s starting from p with length into sub

    @param s String buffer.
    @param sub Substring buffer.
    @param p Starting position.
    @param l Length of the substring.
*/
void substring(char s[], char sub[], int p, int l);

/**
    @brief Fills the given buffer with a timestamp.

    @param ts Buffer to store the timestamp.
    @param len Length of the buffer.
    @return Pointer to the buffer containing the timestamp.
*/
char *timestamp(char *ts, int len);

/**
    @brief Converts a size in bytes to a human-readable format (KB, MB, etc.).

    @param bytes Size in bytes.
    @param sz Buffer to store the human-readable size.
    @param len Length of the buffer.
    @return Pointer to the buffer containing the human-readable size.
*/
char *humansize(uint64_t bytes, char *sz, int len);

/**
    @brief Converts a string to lowercase.

    @param s String to convert.
*/
void toLower(char *s);

#ifdef __cplusplus
}
#endif

#endif // util.h
