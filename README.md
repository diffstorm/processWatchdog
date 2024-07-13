# Process Watchdog

[![Build Status](https://github.com/diffstorm/processWatchdog/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/diffstorm/processWatchdog/actions)
[![License](https://img.shields.io/github/license/diffstorm/processWatchdog)](https://github.com/diffstorm/processWatchdog/blob/main/LICENSE)
[![Language](https://img.shields.io/github/languages/top/diffstorm/processWatchdog)](https://github.com/diffstorm/processWatchdog)

_It will start, monitor and restart applications if they crash or stop sending heartbeat._

The Process Watchdog is a Linux-based utility designed to start, monitor and manage processes specified in a configuration file. It ensures the continuous operation of these processes by periodically checking their status and restarting them if necessary.

## Overview
This application acts as a vigilant guardian for your critical processes, ensuring they remain operational at all times. It accomplishes this task by regularly monitoring the specified processes and taking appropriate actions if any anomalies are detected. The primary function of this application is to ensure that the managed processes remain active, restarting them if they crash or stop sending heartbeat messages.

This application is particularly useful in environments where multiple processes need to be constantly running, such as server systems, clouds or embedded devices. It operates based on the principle that each monitored process should periodically send heartbeat messages. If a process fails to send a heartbeat within the specified time interval, the watchdog manager assumes the process has halted/hang and automatically restarts it.

## Features
- Starts and monitors specified processes listed in a configuration file.
- Restarts processes that have crashed or stopped sending their heartbeat.
- Listens to a specified UDP port for heartbeat messages containing process IDs (PIDs).
- Provides a centralized platform for managing multiple processes, enhancing operational efficiency.
- Provides file command interface to manually start, stop, or restart individual processes or the entire watchdog system or even a Linux reboot.
- Generates statistics log files to track the status and history of each managed process.

## Requirements
- Processes managed by Process Watchdog must periodically send their heartbeat messages to prevent being restarted.
- Configuration file `config.ini` in the same directory with details about the processes to be managed and the UDP port for communication.

## Configuration File : `config.ini`
An example configuration file looks like this:
Note that ping means heartbeat.

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

## Heartbeat Message
A heartbeat message is a UDP packet with the process ID (PID) prefixed by p (e.g., p12345 for PID 12345). It is sent periodically by every managed process to a specified UDP port.

Below are example heartbeat message codes in various languages:

### Java
```java
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

public class ProcessHeartbeat {
    public static void sendPIDOverUDP(int port) {
        try (DatagramSocket socket = new DatagramSocket()) {
            socket.setBroadcast(true);
            
            String pid = "p" + ProcessHandle.current().pid();
            byte[] data = pid.getBytes();
            
            DatagramPacket packet = new DatagramPacket(data, data.length, InetAddress.getByName("127.0.0.255"), port);
            socket.send(packet);            
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
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

void sendPIDOverUDP(int port) {
    int sockfd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    // Get the current process ID
    pid_t pid = getpid();
    char pid_str[20];
    sprintf(pid_str, "p%d", pid);
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    memset(&addr, 0, addr_len);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.255");
    
    if (sendto(sockfd, pid_str, strlen(pid_str), 0, (struct sockaddr *)&addr, addr_len) < 0) {
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
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>

void sendPIDOverUDP(int port) {
    int sockfd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    // Get the current process ID
    pid_t pid = getpid();
    std::string pid_str = "p" + std::to_string(pid);
    const char* pid_data = pid_str.c_str();
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    memset(&addr, 0, addr_len);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.255");
    
    if (sendto(sockfd, pid_data, strlen(pid_data), 0, (struct sockaddr *)&addr, addr_len) < 0) {
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
#include <QCoreApplication>
#include <QUdpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <QProcess>

void sendPIDOverUDP(int port)
{
    QString host = "127.0.0.255";
    QString pid = "p" + QString::number(QCoreApplication::applicationPid());
    QByteArray data = pid.toUtf8();

    QUdpSocket socket;
    socket.bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress);

    socket.writeDatagram(data, QHostAddress(host), port);

    socket.close();
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

public class Program
{
    public static void SendPIDOverUDP(int port)
    {
        try
        {
            string host = "127.0.0.255";
            string pid = "p" + System.Diagnostics.Process.GetCurrentProcess().Id.ToString();
            byte[] data = System.Text.Encoding.ASCII.GetBytes(pid);

            using (UdpClient client = new UdpClient())
            {
                client.EnableBroadcast = true;
                client.Send(data, data.Length, new IPEndPoint(IPAddress.Parse(host), port));
            }
        }
        catch (Exception e)
        {
            Console.WriteLine("Exception: " + e.Message);
        }
    }
}
```
</details>
<details>
  <summary>Click for example in Python</summary>

### Python
```python
import socket
import os

def send_pid_over_udp(port):
    try:
        host = '127.0.0.255'
        pid = f"p{os.getpid()}"
        data = pid.encode()

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.sendto(data, (host, port))
        sock.close()
        
    except Exception as e:
        print(f"Exception: {e}")
```
</details>
<details>
  <summary>Click for example in Shell Script</summary>

### Shell Script
```bash
#!/bin/bash

send_pid_over_udp() {
    local port=$1
    local host="127.0.0.255"
    local pid="p$$"  # $$ gives the PID of the current shell process

    echo -n "$pid" | socat - UDP-DATAGRAM:$host:$port,broadcast
}
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
- Replace ping with heartbeat in the code and .ini
- Redesign the apps.c
- Add CPU & RAM usage to the statistics
- Create easy-to-use heartbeat libraries
- Enable commands over UDP
- Enable remote syslog server reporting
- Add periodic reboot feature
- Add IPC and TCP support
- Add json support
- Add remote Telnet console
- Add config wizard
- Add multiplatform GUI

## :snowman: Author
Eray Öztürk ([@diffstorm](https://github.com/diffstorm))

## LICENSE
This project is licensed under the [GPL-3 License](LICENSE) - see the LICENSE file for details.
