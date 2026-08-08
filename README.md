# Process Watchdog

[![Build Status](https://github.com/diffstorm/processWatchdog/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/diffstorm/processWatchdog/actions)
[![License](https://img.shields.io/github/license/diffstorm/processWatchdog)](https://github.com/diffstorm/processWatchdog/blob/main/LICENSE)
[![Language](https://img.shields.io/github/languages/top/diffstorm/processWatchdog)](https://github.com/diffstorm/processWatchdog)

_It will start, monitor and restart applications if they crash or stop sending heartbeat._

The Process Watchdog is a Linux-based utility designed to start, monitor and manage processes specified in a configuration file. It ensures the continuous operation of these processes by periodically checking their status and restarting them if necessary.

## Overview
This application acts as a vigilant guardian for your critical processes, ensuring they remain operational at all times. It accomplishes this task by regularly monitoring the specified processes and taking appropriate actions if any anomalies are detected. The primary function of this application is to ensure that the managed processes remain active, restarting them if they crash or stop sending heartbeat messages.

This application is particularly useful in environments where multiple processes need to be constantly running, such as server systems, clouds or embedded devices. It operates based on the principle that each monitored process should periodically send heartbeat messages. If a process fails to send a heartbeat within the specified time interval, the watchdog manager assumes the process has halted/hang and automatically restarts it. However there is also support for non-heartbeat sending processes.

## Features
- Starts and monitors specified processes listed in a configuration file.
- Restarts processes that have crashed or stopped sending their heartbeat.
- Listens to a specified UDP port for heartbeat messages containing process IDs (PIDs).
- Provides a centralized platform for managing multiple processes, enhancing operational efficiency.
- Provides file command interface to manually start, stop, or restart individual processes or the entire watchdog system or even a Linux reboot.
- Generates statistics log files to track the status and history of each managed process.
- Monitors CPU and memory usage of each process.
- Supports periodic reboot of the host system.
- Uses monotonic time for heartbeat and restart timing (immune to NTP adjustments and manual clock changes).
- PID reuse detection to prevent false positives when PIDs are recycled.

## Requirements
- If `heartbeat_interval > 0` : Processes managed by Process Watchdog must periodically send their heartbeat messages to prevent being restarted.
- Configuration file `config.ini` in the same directory with details about the processes to be managed and the UDP port for communication.

## Non-heartbeat sending processes
Process Watchdog is also supports non-heartbeat sending processes.
Set the `heartbeat_interval` config to zero to skip heartbeat checks for given processes.

## Progress Signal (Stagnation Detection)
By default, the watchdog only checks if a process is **alive** (sending heartbeats). A process can be alive but stuck in an infinite loop or deadlocked, wasting resources without doing useful work.

The **progress signal** feature detects this stagnation by requiring the process to explicitly signal when it completes a unit of work.

### How It Works
- **Liveness Signal**: `p<pid>` - Process is running
- **Progress Signal**: `p<pid>+` - Process is running AND completed work

The watchdog tracks the time since the last progress signal. If `progress_timeout` is exceeded, the process is restarted.

**Best Practices:**
- Progress timeout should be > heartbeat interval (auto-adjusted if needed)
- Reasonable values: 60-3600 seconds (1 minute to 1 hour)
- Maximum allowed: 7 days (604800 seconds)

### Configuration
Add `progress_timeout` to your app section:

```ini
[app:Bot]
start_delay = 20
heartbeat_delay = 90
heartbeat_interval = 30
progress_timeout = 60  # Restart if no progress signal in 60 seconds
cmd = /usr/bin/python bot.py
```

- `progress_timeout = 0` (default): Stagnation checking disabled (backward compatible)
- `progress_timeout > 0`: Maximum seconds between progress signals

### Sending Progress Signals
Append `+` to the heartbeat when work completes:

**Python:**
```python
import socket
import os

def send_pid_over_udp(port, send_progress=False):
    host = '127.0.0.255'
    pid = f"p{os.getpid()}"
    if send_progress:
        pid += "+"  # Progress signal: process completed a unit of work
    data = pid.encode()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    sock.sendto(data, (host, port))
    sock.close()

# Liveness only
send_pid_over_udp(12345)

# After completing a work unit
send_pid_over_udp(12345, send_progress=True)
```

**C:**
```c
void sendPIDOverUDP(int port, bool send_progress) {
    pid_t pid = getpid();
    char pid_str[20];
    
    if (send_progress) {
        sprintf(pid_str, "p%d+", pid);  // Progress signal
    } else {
        sprintf(pid_str, "p%d", pid);   // Liveness only
    }
    // ... send via UDP
}

sendPIDOverUDP(12345, false);  // Liveness only
sendPIDOverUDP(12345, true);   // After work completes
```

### Test Child Script
Send progress signals by providing an optional third argument: the interval in seconds. If > 0, sends `p<pid>+` every N seconds.

```bash
# No progress signal (backward compatible)
python test_child.py 1 crash

# Send progress signal every 45 seconds (works with crash role)
python test_child.py 1 crash 45

# No heartbeat but send progress every 60 seconds (will trigger progress_timeout restart in watchdog)
python test_child.py 2 noheartbeat 60

# No heartbeat, no progress - just runs until max_runtime
python test_child.py 4 noheartbeat
```

## Configuration File : `config.ini`
An example configuration file looks like this:

```ini
[processWatchdog]
udp_port = 12345

[app:Communicator]
start_delay = 10
heartbeat_delay = 60
heartbeat_interval = 20
cmd = /usr/bin/python test_child.py 1 crash
# Restart policy with exponential backoff (optional)
# max_retries = 5
# base_delay = 1
# stop_timeout = 30

[app:Bot]
start_delay = 20
heartbeat_delay = 90
heartbeat_interval = 30
cmd = /usr/bin/python test_child.py 2 noheartbeat
# No heartbeat at all (tests watchdog restart when heartbeat is missing)
# Restart policy with exponential backoff (optional)
# max_retries = 5
# base_delay = 1
# stop_timeout = 30

[app:Publisher]
start_delay = 35
heartbeat_delay = 70
heartbeat_interval = 16
cmd = /usr/bin/python test_child.py 3 crash
# Restart policy with exponential backoff (optional)
# max_retries = 5
# base_delay = 1
# stop_timeout = 30

[app:Alert]
start_delay = 35
heartbeat_delay = 130
heartbeat_interval = 13
cmd = /usr/bin/python test_child.py 4 noheartbeat
# Restart policy with exponential backoff (optional)
# max_retries = 5
# base_delay = 1
# stop_timeout = 30

[app:BackoffTest]
start_delay = 5
heartbeat_delay = 10
heartbeat_interval = 120
cmd = /usr/bin/python test_child.py 5 crash
# Restart policy with exponential backoff
max_retries = 3
base_delay = 2
stop_timeout = 30

[app:ProgressTest]
start_delay = 5
heartbeat_delay = 10
heartbeat_interval = 15
progress_timeout = 30
cmd = /usr/bin/python test_child.py 6 crash 10
# Sends heartbeat and progress signal (+) every 10 seconds
# progress_timeout=30 means restart if no progress in 30 seconds
# This tests the progress feature: process must send p<pid>+ within 30s to avoid restart
```

### Fields
- `udp_port` : The UDP port to expect heartbeats.
- `periodic_reboot`: Optional. Specifies the periodic reboot schedule. The feature is disabled if the key is not found or the value is invalid. The supported formats are:
    - Daily Specific Time: A specific time in `HH:MM` format (e.g., `04:00` for a reboot at 4:00 AM every day).
    - Hourly Interval: An integer value followed by `h` (e.g., `6h` for every 6 hours).
    - Daily Interval: An integer value followed by `d` (e.g., `5d` for every 5 days).
    - Weekly Interval: An integer value followed by `w` (e.g., `2w` for every 2 weeks).
    - Monthly Interval: An integer value followed by `m` (e.g., `3m` for every 3 months).
    - Default to Days: If no unit is provided, the value is treated as days (e.g., `7` is equivalent to `7d`).
- `[app:<AppName>]` : Each application to be monitored should have its own section prefixed with `app:`. `<AppName>` will be used as the name of the application.
- `start_delay` : Delay in seconds before starting the application.
- `heartbeat_delay` : Time in seconds to wait before expecting a heartbeat from the application.
- `heartbeat_interval` : Maximum time period in seconds between heartbeats (`0`:disables heartbeat checks).
- `progress_timeout` : Optional. Maximum seconds between progress signals (`0`:disables stagnation checking). See [Progress Signal](#progress-signal-stagnation-detection) section.
- `cmd` : Command to start the application.
- `max_retries` : Optional. Maximum number of retry attempts for restart policy. Set to `0` to disable backoff (current behavior - immediate restart). Default: `0`.
- `base_delay` : Optional. Base delay in seconds for exponential backoff calculation. Only used when `max_retries > 0`. Default: `1`.
- `stop_timeout` : Optional. Seconds to wait for graceful shutdown before sending SIGKILL. Default: `10`.

### Circuit-Breaker & Cooldown (Anti-Flap)

These features prevent cascading failures in production bot fleets.

#### Circuit-Breaker

Prevents cascading failures by opening circuit after repeated failures.

```ini
circuit_breaker_threshold = 5    # Open after 5 failures
circuit_breaker_window = 300     # In 5 minute window
```

- `0` = disabled (default)
- N > 0 = open circuit after N failures in window

**Behavior:**
- CLOSED: Normal operation, accepts restarts
- OPEN: Circuit triggered after N failures, blocks restarts
- HALF_OPEN: After 2x window expires, allows one trial restart
- On success: Resets to CLOSED; on failure: Re-opens

#### Cooldown

Prevents rapid restart loops with waiting period.

```ini
cooldown_period = 120              # Wait 2 minutes
cooldown_manual_ack_threshold = 3  # Require ack after 3 cooldowns
```

- `cooldown_period = 0` = disabled (default)
- `cooldown_period > 0` = wait N seconds before auto-resume
- `cooldown_manual_ack_threshold = 0` = auto-resume always
- `cooldown_manual_ack_threshold > 0` = require manual ack after N cooldowns

**Manual Ack File Command:** Create `resume<app>` to manually resume.

#### Testing

Use `FlapTest` and `CircuitTest` in `config.ini` to test features:

```bash
# Circuit-breaker test: 3 failures in 5 min, cooldown with ack
python test_child.py 7 crash

# Circuit-breaker test: 2 failures in 5 min, auto-resume
python test_child.py 8 crash
```

### Restart Policy with Exponential Backoff

The Process Watchdog supports a configurable restart policy with exponential backoff to prevent rapid restart loops when a process consistently fails to start.

#### Configuration

Add `max_retries` and `base_delay` to your app configuration:

```ini
[app:MyApp]
start_delay = 10
heartbeat_delay = 60
heartbeat_interval = 20
cmd = /usr/bin/python myapp.py
max_retries = 5
base_delay = 1
```

For a practical test example, see the `BackoffTest` app in `config.ini`:

```ini
[app:BackoffTest]
start_delay = 5
heartbeat_delay = 10
heartbeat_interval = 60
cmd = /usr/bin/python test_child.py 5 crash
max_retries = 3
base_delay = 2
```

#### How It Works

- **Backoff Disabled (default)**: When `max_retries` is `0` or not specified, the watchdog immediately restarts failed processes (current behavior).
- **Backoff Enabled**: When `max_retries > 0`, the watchdog implements exponential backoff:
  - On failure, the retry count is incremented
  - The delay before the next restart is calculated as: `delay = base_delay * 2^(retry_count - 1)`
  - Maximum delay is capped at 3600 seconds (1 hour)
  - When `retry_count` reaches `max_retries`, permanent failure is logged and retries stop
  - On successful start, retry count is reset to `0`

#### Example Backoff Delays

With `base_delay = 1`:
- 1st retry: 1 second delay (2^0)
- 2nd retry: 2 seconds delay (2^1)
- 3rd retry: 4 seconds delay (2^2)
- 4th retry: 8 seconds delay (2^3)
- 5th retry: 16 seconds delay (2^4), capped at 3600s if higher

#### Behavior

- **Retry Count**: Incremented on process failure, reset to `0` on successful start
- **Failure Logging**: When max retries reached, logs permanent failure with retry count
- **Backoff Logging**: During backoff wait, logs remaining delay time

### Circuit-Breaker and Cooldown

The watchdog supports circuit-breaker and cooldown features for production bot fleets to prevent cascading failures.

#### Circuit-Breaker

Circuit-breaker prevents restart attempts after repeated failures within a time window:

```ini
[app:Bot]
start_delay = 20
heartbeat_delay = 90
heartbeat_interval = 30
cmd = /usr/bin/python bot.py

# Circuit-breaker: 5 failures in 5 minutes opens circuit
circuit_breaker_threshold = 5
circuit_breaker_window = 300
```

**Behavior:**
- Circuit starts `CLOSED`, accepts restarts
- After `circuit_breaker_threshold` failures in window, circuit `OPEN`s
- No restarts while `OPEN`
- After `2x circuit_breaker_window`, circuit goes `HALF_OPEN`
- One trial restart allowed; if success, resets to `CLOSED`

#### Cooldown

Cooldown adds a waiting period after circuit opens, with optional manual acknowledgment:

```ini
[app:Bot]
# Cooldown: wait 2 minutes before auto-resume
cooldown_period = 120
# Require manual ack after 3 cooldowns
cooldown_manual_ack_threshold = 3
```

**Behavior:**
- `cooldown_period = 0`: Disabled (no cooldown)
- `cooldown_period > 0`: Wait period after circuit opens
- `cooldown_manual_ack_threshold = 0`: Auto-resume after cooldown
- `cooldown_manual_ack_threshold > 0`: Require manual ack after N cooldowns

#### File Command for Manual Ack

Create `resume<app>` file to manually acknowledge and resume:

```bash
touch resumebot    # Resumes the Bot app
```

The watchdog detects this file, removes it, and resumes the application.

#### Complete Anti-Flap Example

```ini
[app:Bot]
start_delay = 20
heartbeat_delay = 90
heartbeat_interval = 30
progress_timeout = 60
cmd = /usr/bin/python bot.py

# Circuit-breaker
circuit_breaker_threshold = 5
circuit_breaker_window = 300

# Cooldown
cooldown_period = 120
cooldown_manual_ack_threshold = 3
```

## Heartbeat Message
For heartbeat sending processes : A heartbeat message is a UDP packet with the process ID (`PID`) prefixed by `p` (e.g., `p12345` for PID `12345`). It is sent periodically by every managed process to a specified UDP port.

Below are example heartbeat message codes in various languages:

### Java
```java
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

public class ProcessHeartbeat {
    public static void sendPIDOverUDP(int port, boolean sendProgress) {
        try (DatagramSocket socket = new DatagramSocket()) {
            socket.setBroadcast(true);
            
            long pid = ProcessHandle.current().pid();
            String pidStr = "p" + pid;
            if (sendProgress) {
                pidStr += "+";  // Progress signal
            }
            byte[] data = pidStr.getBytes();
            
            DatagramPacket packet = new DatagramPacket(data, data.length, InetAddress.getByName("127.0.0.255"), port);
            socket.send(packet);            
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}
```

Usage:
```java
sendPIDOverUDP(12345, false);  // Liveness only
sendPIDOverUDP(12345, true);   // Liveness + progress
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

void sendPIDOverUDP(int port, bool send_progress) {
    int sockfd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    pid_t pid = getpid();
    char pid_str[20];
    
    if (send_progress) {
        sprintf(pid_str, "p%d+", pid);  // Progress signal
    } else {
        sprintf(pid_str, "p%d", pid);   // Liveness only
    }
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    memset(&addr, 0, addr_len);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.255");
    
    sendto(sockfd, pid_str, strlen(pid_str), 0, (struct sockaddr *)&addr, addr_len);
    close(sockfd);
}
```

Usage:
```c
sendPIDOverUDP(12345, false);  // Liveness only
sendPIDOverUDP(12345, true);   // Liveness + progress
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

void sendPIDOverUDP(int port, bool send_progress) {
    int sockfd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    pid_t pid = getpid();
    std::string pid_str = "p" + std::to_string(pid);
    if (send_progress) {
        pid_str += "+";  // Progress signal
    }
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    memset(&addr, 0, addr_len);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr("127.0.0.255");
    
    sendto(sockfd, pid_str.c_str(), pid_str.length(), 0, (struct sockaddr *)&addr, addr_len);
    close(sockfd);
}
```

Usage:
```cpp
sendPIDOverUDP(12345, false);  // Liveness only
sendPIDOverUDP(12345, true);   // Liveness + progress
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

void sendPIDOverUDP(int port, bool sendProgress)
{
    QString host = "127.0.0.255";
    QString pid = "p" + QString::number(QCoreApplication::applicationPid());
    if (sendProgress) {
        pid += "+";  // Progress signal
    }
    QByteArray data = pid.toUtf8();

    QUdpSocket socket;
    socket.bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress);

    socket.writeDatagram(data, QHostAddress(host), port);
    socket.close();
}
```

Usage:
```cpp
sendPIDOverUDP(12345, false);  // Liveness only
sendPIDOverUDP(12345, true);   // Liveness + progress
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
    public static void SendPIDOverUDP(int port, bool sendProgress)
    {
        try
        {
            string host = "127.0.0.255";
            string pid = "p" + System.Diagnostics.Process.GetCurrentProcess().Id.ToString();
            if (sendProgress)
            {
                pid += "+";  // Progress signal
            }
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

Usage:
```csharp
SendPIDOverUDP(12345, false);  // Liveness only
SendPIDOverUDP(12345, true);   // Liveness + progress
```
</details>
<details>
  <summary>Click for example in Python</summary>

### Python
```python
import socket
import os

def send_pid_over_udp(port, send_progress=False):
    try:
        host = '127.0.0.255'
        pid = f"p{os.getpid()}"
        if send_progress:
            pid += "+"  # Progress signal: process completed a unit of work
        data = pid.encode()

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.sendto(data, (host, port))
        sock.close()
        
    except Exception as e:
        print(f"Exception: {e}")
```

Usage:
```python
send_pid_over_udp(12345)              # Liveness only
send_pid_over_udp(12345, send_progress=True)  # Liveness + progress
```
</details>
<details>
  <summary>Click for example in Rust</summary>

### Rust
```rust
use std::net::{UdpSocket, ToSocketAddrs};

fn send_pid_over_udp(port: u16, send_progress: bool) {
    let host = "127.0.0.255";
    let pid = std::process::id();
    let mut pid_str = format!("p{}", pid);
    
    if send_progress {
        pid_str += "+";  // Progress signal
    }
    
    let data = pid_str.as_bytes();
    
    match UdpSocket::bind("0.0.0.0:0") {
        Ok(socket) => {
            socket.set_broadcast(true).unwrap();
            let addr = (host, port).to_socket_addrs().unwrap().next().unwrap();
            socket.send_to(data, addr).unwrap();
        }
        Err(e) => {
            eprintln!("Failed to bind socket: {}", e);
        }
    }
}
```

Usage:
```rust
send_pid_over_udp(12345, false);  // Liveness only
send_pid_over_udp(12345, true);   // Liveness + progress
```
</details>
<details>
  <summary>Click for example in Go</summary>

### Go
```go
package main

import (
    "fmt"
    "net"
    "os"
    "strconv"
)

func sendPIDOverUDP(port int, sendProgress bool) {
    host := "127.0.0.255"
    pid := os.Getpid()
    pidStr := fmt.Sprintf("p%d", pid)
    
    if sendProgress {
        pidStr += "+"  // Progress signal
    }
    
    data := []byte(pidStr)
    
    addr, err := net.ResolveUDPAddr("udp", fmt.Sprintf("%s:%d", host, port))
    if err != nil {
        fmt.Printf("ResolveUDPAddr error: %v\n", err)
        return
    }
    
    conn, err := net.ListenPacket("udp", ":0")
    if err != nil {
        fmt.Printf("ListenPacket error: %v\n", err)
        return
    }
    defer conn.Close()
    
    conn.(*net.UDPConn).SetBroadcast(true)
    conn.WriteTo(data, addr)
}
```

Usage:
```go
sendPIDOverUDP(12345, false)  // Liveness only
sendPIDOverUDP(12345, true)   // Liveness + progress
```
</details>
<details>
  <summary>Click for example in TypeScript/Node.js</summary>

### TypeScript/Node.js
```typescript
import dgram from 'dgram';
import os from 'os';

function sendPIDOverUDP(port: number, sendProgress: boolean): void {
    const host = '127.0.0.255';
    const pid = process.pid;
    let pidStr = `p${pid}`;
    
    if (sendProgress) {
        pidStr += '+';  // Progress signal
    }
    
    const client = dgram.createSocket('udp4');
    client.bind(() => {
        client.setBroadcast(true);
        const buffer = Buffer.from(pidStr);
        client.send(buffer, 0, buffer.length, port, host, (err) => {
            if (err) {
                console.error(`Send error: ${err.message}`);
            }
            client.close();
        });
    });
}
```

Usage:
```typescript
sendPIDOverUDP(12345, false);  // Liveness only
sendPIDOverUDP(12345, true);   // Liveness + progress
```
</details>
<details>
  <summary>Click for example in Ruby</summary>

### Ruby
```ruby
require 'socket'

def send_pid_over_udp(port, send_progress = false)
    host = '127.0.0.255'
    pid = Process.pid
    pid_str = "p#{pid}"
    
    if send_progress
        pid_str += '+'  # Progress signal
    end
    
    begin
        sock = UDPSocket.new
        sock.setsockopt(Socket::SOL_SOCKET, Socket::SO_BROADCAST, 1)
        sock.send(pid_str, 0, host, port)
        sock.close
    rescue => e
        puts "Exception: #{e.message}"
    end
end
```

Usage:
```ruby
send_pid_over_udp(12345)              # Liveness only
send_pid_over_udp(12345, true)        # Liveness + progress
```
</details>
<details>
  <summary>Click for example in Swift</summary>

### Swift
```swift
import Foundation

func sendPIDOverUDP(port: Int, sendProgress: Bool) {
    let host = "127.0.0.255"
    let pid = getpid()
    var pidStr = "p\(pid)"
    
    if sendProgress {
        pidStr += "+"  // Progress signal
    }
    
    guard let data = pidStr.data(using: .utf8) else {
        print("Failed to encode data")
        return
    }
    
    let socket = GCDAsyncUdpSocket(delegate: nil, delegateQueue: nil)
    
    do {
        try socket.enableBroadcast()
        let address = try socket.address(from: host, port: port)
        socket.send(data, to: address, withTimeout: -1, tag: 0)
    } catch {
        print("Error: \(error.localizedDescription)")
    }
}
```

Usage:
```swift
sendPIDOverUDP(port: 12345, sendProgress: false)  // Liveness only
sendPIDOverUDP(port: 12345, sendProgress: true)   // Liveness + progress
```
</details>
<details>
  <summary>Click for example in PHP</summary>

### PHP
```php
<?php

function sendPIDOverUDP($port, $sendProgress = false) {
    $host = '127.0.0.255';
    $pid = getmypid();
    $pidStr = "p{$pid}";
    
    if ($sendProgress) {
        $pidStr .= '+';  // Progress signal
    }
    
    $data = $pidStr;
    
    $socket = socket_create(AF_INET, SOCK_DGRAM, SOL_UDP);
    if ($socket === false) {
        error_log("Failed to create socket: " . socket_strerror(socket_last_error()));
        return;
    }
    
    $broadcast = true;
    if (!socket_set_option($socket, SOL_SOCKET, SO_BROADCAST, $broadcast)) {
        error_log("Failed to enable broadcast: " . socket_strerror(socket_last_error($socket)));
        socket_close($socket);
        return;
    }
    
    if (socket_sendto($socket, $data, strlen($data), 0, $host, $port) === false) {
        error_log("Failed to send data: " . socket_strerror(socket_last_error($socket)));
    }
    
    socket_close($socket);
}

// Usage:
sendPIDOverUDP(12345, false);  // Liveness only
sendPIDOverUDP(12345, true);   // Liveness + progress
?>
```
</details>
<details>
  <summary>Click for example in Shell Script</summary>

### Shell Script
```bash
#!/bin/bash

send_pid_over_udp() {
    local port=$1
    local send_progress=$2
    local host="127.0.0.255"
    local pid="p$$"  # $$ gives the PID of the current shell process
    
    if [ "$send_progress" = "true" ]; then
        pid="${pid}+"  # Progress signal
    fi
    
    echo -n "$pid" | socat - UDP-DATAGRAM:$host:$port,broadcast
}

# Liveness only
send_pid_over_udp 12345 false

# With progress signal
send_pid_over_udp 12345 true
```
</details>

## Statistics Logging
The application generates log files to monitor the status of each managed process. Example log entry:

```
Statistics for App 2 Publisher:
Started at: 2024-06-07 20:17:10
Crashed at: Never
Heartbeat reset at: Never
Start count: 7
Crash count: 0
Heartbeat reset count: 0
Heartbeat count: 11937
Heartbeat count old: 15455
Average first heartbeat time: 105 seconds
Maximum first heartbeat time: 107 seconds
Minimum first heartbeat time: 104 seconds
Average heartbeat time: 102 seconds
Maximum heartbeat time: 110 seconds
Minimum heartbeat time: 102 seconds
Resource sample count: 190
Current CPU usage: 0.50%
Maximum CPU usage: 0.80%
Minimum CPU usage: 0.00%
Average CPU usage: 0.40%
Current memory usage: 11.62 MB
Maximum memory usage: 11.88 MB
Minimum memory usage: 11.38 MB
Average memory usage: 11.74 MB
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

## Building with CMake

To build the project with CMake, you need to have CMake and GTest installed.

    ```bash
    mkdir build
    cd build
    cmake ..
    make
    ./test/unit_tests
    ```

## Systemd Service Installation (optional)

For production deployment on systemd-based Linux systems, you can install processWatchdog as a system service:

1. **Build and install the binary:**
   ```bash
   make BUILD_TYPE=release
   sudo cp processWatchdog /usr/local/bin/
   sudo chmod +x /usr/local/bin/processWatchdog
   ```

2. **Create configuration directory:**
   ```bash
   sudo mkdir -p /etc/processwatchdog
   sudo mkdir -p /var/lib/processwatchdog
   sudo cp config.ini /etc/processwatchdog/
   ```

3. **Install the service file:**
   ```bash
   sudo cp processwatchdog.service /etc/systemd/system/
   sudo systemctl daemon-reload
   ```

4. **Enable and start the service:**
   ```bash
   sudo systemctl enable processwatchdog
   sudo systemctl start processwatchdog
   ```

5. **Check service status:**
   ```bash
   sudo systemctl status processwatchdog
   sudo journalctl -u processwatchdog -f  # Follow logs
   ```

**Note:** Edit `/etc/processwatchdog/config.ini` and adjust paths in `processwatchdog.service` as needed for your setup.

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
- Attaching to already running processes
- Enable commands over UDP
- Enable remote syslog server reporting
- Add periodic server health reporting
- Add IPC and TCP support
- Add json support
- Configurable timeouts (resource sampling interval, stats write interval, process termination timeout)

## :snowman: Author
Eray Öztürk ([@diffstorm](https://github.com/diffstorm))

## LICENSE
This project is licensed under the [GPL-3 License](LICENSE) - see the LICENSE file for details.
