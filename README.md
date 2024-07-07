# Process Watchdog

[![Build Status](https://github.com/diffstorm/processWatchdog/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/diffstorm/processWatchdog/actions)
[![License](https://img.shields.io/github/license/diffstorm/processWatchdog)](https://github.com/diffstorm/processWatchdog/blob/main/LICENSE)
[![Language](https://img.shields.io/github/languages/top/diffstorm/processWatchdog)](https://github.com/diffstorm/processWatchdog)

The Process Watchdog is a Linux-based utility designed to start, monitor and manage processes specified in a configuration file. It ensures the continuous operation of these processes by periodically checking their status and restarting them if necessary.

## Overview
This application acts as a vigilant guardian for your critical processes, ensuring they remain operational at all times. It accomplishes this task by regularly monitoring the specified processes and taking appropriate actions if any anomalies are detected. The primary function of this application is to ensure that the managed processes remain active, restarting them if they crash or stop sending heartbeat signals over UDP.

This application is particularly useful in environments where multiple processes need to be constantly running, such as server systems or embedded devices. It operates based on the principle that each monitored process should periodically send its process ID (PID) over UDP. If a process fails to send its PID within the specified time interval, the watchdog manager assumes the process has crashed and automatically restarts it.

## Features
- Starts and monitors specified processes listed in a configuration file.
- Restarts processes that have crashed or stopped sending their PID.
- Listens to a specified UDP port for heartbeat messages containing process IDs (PIDs).
- Provides a centralized platform for managing multiple processes, enhancing operational efficiency.
- Provides file command interface to manually start, stop, or restart individual processes or the entire watchdog system or even a Linux reboot.
- Generates statistics log files to track the status and history of each managed process.

## Requirements
- Processes managed by Process Watchdog must periodically send their PID over UDP to prevent being restarted.
- Configuration file `config.ini` in the same directory with details about the processes to be managed and the UDP port for communication.

## Configuration File : `config.ini`
An example configuration file looks like this:

```ini
[processWatchdog]
udp_port = 12345
nWdtApps = 4

1_name = Communicator
1_start_delay = 10
1_ping_delay = 60
1_ping_interval = 20
1_cmd = /usr/bin/python test_child.py 1 crash

2_name = Bot
2_start_delay = 20
2_ping_delay = 90
2_ping_interval = 30
2_cmd = /usr/bin/python test_child.py 2 noping

3_name = Publisher
3_start_delay = 35
3_ping_delay = 70
3_ping_interval = 16
3_cmd = /usr/bin/python test_child.py 3 crash

4_name = Alert
4_start_delay = 35
4_ping_delay = 130
4_ping_interval = 13
4_cmd = /usr/bin/python test_child.py 4 noping
```

### Fields
- `name` : Name of the application.
- `start_delay` : Delay in seconds before starting the application.
- `ping_delay` : Time in seconds to wait before expecting a ping from the application.
- `ping_interval` : Maximum time period in seconds between pings.
- `cmd` : Command to start the application.

## Example Heartbeat Message Code
The managed processes must send a message containing their PID over UDP. Below are example heartbeat message codes in various languages.

### Java
```java
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

public class ProcessHeartbeat {
    public static void sendPIDOverUDP(int port) {
        try {
            String host = "127.0.0.255";
            String pid = "p" + Long.toString(ProcessHandle.current().pid());
            byte[] data = pid.getBytes();
            DatagramSocket socket = new DatagramSocket();
            DatagramPacket packet = new DatagramPacket(data, data.length, InetAddress.getByName(host), port);
            socket.send(packet);
            socket.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
```

<details>
  <summary>Click for example in C</summary>

### C
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

void sendPIDOverUDP(int port) {
    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "p%d", getpid());

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_BROADCAST;

    // Send the PID message
    if (sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("sendto failed");
    }

    close(sockfd);
}
```
</details>
<details>
  <summary>Click for example in C++</summary>

### C++
```cpp
#include <iostream>
#include <unistd.h>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

void sendPIDOverUDP(int port) {
    int sockfd;
    struct sockaddr_in servaddr;
    std::string pid_message = "p" + std::to_string(getpid());

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_BROADCAST;

    // Send the PID message
    if (sendto(sockfd, pid_message.c_str(), pid_message.length(), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("sendto failed");
    }

    close(sockfd);
}
```
</details>
<details>
  <summary>Click for example in Qt</summary>

### Qt (C++)
```cpp
#include <QUdpSocket>
#include <QCoreApplication>
#include <QProcess>

void sendPIDOverUDP(int port) {
    QUdpSocket udpSocket;
    QString message = "p" + QString::number(QCoreApplication::applicationPid());
    QByteArray data = message.toUtf8();

    udpSocket.writeDatagram(data, QHostAddress::Broadcast, port);
}
```
</details>
<details>
  <summary>Click for example in C#</summary>

### C#
```csharp
using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Diagnostics;

public class Program
{
    public static void SendPIDOverUDP(int port)
    {
        UdpClient udpClient = new UdpClient();
        int pid = Process.GetCurrentProcess().Id;
        string message = "p" + pid.ToString();
        byte[] data = Encoding.UTF8.GetBytes(message);

        IPEndPoint endPoint = new IPEndPoint(IPAddress.Broadcast, port);
        udpClient.Send(data, data.Length, endPoint);
        udpClient.Close();
    }

    public static void Main()
    {
        SendPIDOverUDP(12345);
    }
}
```
</details>
<details>
  <summary>Click for example in Python</summary>

### Python
```python
import os
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
pid = str(os.getpid())
data = 'p' + pid
sock.sendto(data.encode('utf-8'), ('localhost', 12345))
```
</details>
<details>
  <summary>Click for example in Shell Script</summary>

### Shell Script
```bash
#!/bin/bash

sendPIDOverUDP() {
    local port=$1
    local pid="p$$"
    echo -n $pid | nc -u -w1 -b 127.0.0.1 $port
}

sendPIDOverUDP 12345
```
</details>

## Statistics Logging
The application generates log files to monitor the status of each managed process. Example log entry:

```
Statistics for App 2 Publisher:
Started at: 2024-06-07 20:17:10
Crashed at: Never
Ping reset at: Never
Start count: 7
Crash count: 0
Ping reset count: 0
Ping count: 11937
Ping count old: 15455
Average first ping time: 105 seconds
Maximum first ping time: 107 seconds
Minimum first ping time: 104 seconds
Average ping time: 102 seconds
Maximum ping time: 110 seconds
Minimum ping time: 102 seconds
Magic: A50FAA55
```

## File Commands
Process Watchdog can be controlled using file commands:

- **Control all processes or the main app:**
  - `wdtstop`: Stop all applications and then itself.
  - `wdtrestart`: Restart all applications and itself.
  - `wdtreboot`: Reboot the system.

- **Control individual applications specified in the ini file:**
  - `stop<app>`: Stop the specified application.
  - `start<app>`: Start the specified application if it is not running.
  - `restart<app>`: Restart the specified application.

## Compilation
A `Makefile` is included to compile the Process Watchdog application.

```bash
make
```

## Running the Application
Use the provided `run.sh` script to start the Process Watchdog application. This script includes a mechanism to restart the watchdog itself if it crashes, providing an additional level of protection.

## Usage
```bash
./processWatchdog -i <file.ini> [-v] [-h] [-t testname]
```

- `-i <file.ini>`: Specify the configuration file.
- `-v`: Display version information.
- `-h`: Display help information.
- `-t <testname>`: Run unit tests.

Or just `./run.sh &` which is recommended.

## TODO
- Redesign the apps.c
- Replace ini with json file
- Add CPU & RAM usage to the statistics
- Add Telnet console
- Enable remote syslog server reporting
- Enable commands over UDP
- Add IPC and TCP support

## :snowman: Author
Eray Öztürk ([@diffstorm](https://github.com/diffstorm))

## LICENSE
This project is licensed under the [GPL-3 License](LICENSE) - see the LICENSE file for details.
