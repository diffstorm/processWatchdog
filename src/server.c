/**
    @file server.c
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

#include "log.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>

static struct pollfd pfd;

int udp_start(int *socketfd, int port)
{
    struct sockaddr_in si_me;
    // create a UDP socket
    *socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if(*socketfd == -1)
    {
        LOGE("socket could not be created");
        return 1;
    }

    // set socket options to reuse address and port
    int optval = 1;

    if(setsockopt(*socketfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        LOGE("setsockopt error");
        return 1;
    }

    // bind socket to the specified port
    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(*socketfd, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
    {
        LOGE("bind error");
        return 1;
    }

    signal(SIGCHLD, SIG_IGN);
    // Ignore SIGPIPE trigged when sending data to an already closed socket.
    signal(SIGPIPE, SIG_IGN);
    LOGI("UDP server started on port %d", port);
    // set up the pollfd structure for listening on the socket
    pfd.fd = *socketfd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    return 0;
}

static int udp_pollp(int socketfd, struct pollfd *pfd, int timeout, char *data, int *len)
{
    struct sockaddr_in si_other;
    int slen = sizeof(si_other), recv_len, data_len;
    data_len = *len;
    *len = 0;

    // call poll() to wait for events on the socket
    if(poll(pfd, 1, timeout) == -1)
    {
        if(errno != EINTR) // Interrupted system call
        {
            LOGE("poll error, error : %d - %s", errno, strerror(errno));
        }

        return 1;
    }

    // check if there is data to read on the socket
    if(pfd->revents & POLLIN)
    {
        // receive a message from a client
        recv_len = recvfrom(socketfd, data, data_len, 0, (struct sockaddr *) &si_other, &slen);

        if(recv_len > 0)
        {
            if(recv_len > data_len)
            {
                LOGE("Error : recv_len %d > data_len %d", recv_len, data_len);
                recv_len = data_len - 1;
            }

            data[recv_len] = 0; // Add a string terminator
        }

        if(recv_len == -1 && errno == EAGAIN)
        {
            LOGE("recvfrom errno == EAGAIN");
            return 0;
        }

        if(recv_len == -1)
        {
            LOGE("recvfrom error");
            return 1;
        }

        *len = recv_len;
        // print the received message
        LOGD("UDP received from %s:%d - %.*s", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), recv_len, data);
    }

    return 0;
}

int udp_poll(int socketfd, int timeout, char *data, int *len)
{
    return udp_pollp(socketfd, &pfd, timeout, data, len);
}

void udp_stop(int socketfd)
{
    LOGD("Stopping UDP server...");
    fsync(socketfd);
    close(socketfd);
    LOGI("UDP server stopped");
}
