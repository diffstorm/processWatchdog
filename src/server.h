/**
    @file server.h
    @brief Process Watchdog Application Manager

    The Process Watchdog application manages the processes listed in the configuration file.
    It listens to a specified UDP port for heartbeat messages from these processes, which must
    periodically send their PID. If any process stops running or fails to send its PID over UDP
    within the expected interval, the Process Watchdog application will restart the process.

    The application ensures high reliability and availability by continuously monitoring and
    restarting processes as necessary. It also logs various statistics about the monitored
    processes, including start times, crash times, and ping intervals.

    @date 2023-01-01
    @version 1.0
    @author by Eray Ozturk | erayozturk1@gmail.com
    @url github.com/diffstorm
    @license GPL-3 License
*/

#ifndef SERVER_H
#define SERVER_H

/**
    @file server.h
    @brief Functions for managing UDP server operations.
*/

/**
    @brief Starts a UDP server on the specified port.

    @param socketfd Pointer to store the socket file descriptor.
    @param port The port number to bind the server to.
    @return 0 on success, else on failure.
*/
int udp_start(int *socketfd, int port);

/**
    @brief Polls the UDP server for incoming data.

    @param socketfd The socket file descriptor of the UDP server.
    @param timeout The timeout value for polling in milliseconds.
    @param data Pointer to store the received data.
    @param len Pointer to store the length of the received data.
    @return 0 on success, else on failure.
*/
int udp_poll(int socketfd, int timeout, char *data, int *len);

/**
    @brief Stops the UDP server and closes the socket.

    @param socketfd The socket file descriptor of the UDP server.
*/
void udp_stop(int socketfd);

#endif // SERVER_H

