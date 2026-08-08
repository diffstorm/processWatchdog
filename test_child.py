# processWatchdog application test child python script
# Copyright (c) 2023 Eray Ozturk <erayozturk1@gmail.com>

# Usage:
# python test_child <index> <role> [progress_interval]
# python test_child 1 crash                        # No progress signal
# python test_child 2 noheartbeat                  # No heartbeat at all
# python test_child 3 crash 45                     # Send progress signal every 45 seconds
# python test_child 7 crash                        # Circuit-breaker test (no progress, default config)

import os
import socket
import random
import time
import sys
import configparser

# Default values
appname = sys.argv[0]
role = "crash"
index = 1
progress_interval = 0  # 0 = no progress signal, >0 = send + every N seconds

COLOR_TABLE = [
    ("\033[48;2;255;87;51m", "\033[38;2;255;255;255m"),  # Vibrant Red bg, White text
    ("\033[48;2;46;204;113m", "\033[38;2;0;0;0m"),  # Emerald Green bg, Black text
    ("\033[48;2;241;196;15m", "\033[38;2;0;0;0m"),  # Bright Yellow bg, Black text
    ("\033[48;2;52;152;219m", "\033[38;2;255;255;255m"),  # Sky Blue bg, White text
    ("\033[48;2;155;89;182m", "\033[38;2;255;255;255m"),  # Amethyst bg, White text
    ("\033[48;2;26;188;156m", "\033[38;2;0;0;0m"),  # Turquoise bg, Black text
    ("\033[48;2;231;76;60m", "\033[38;2;255;255;255m"),  # Strong Red bg, White text
    ("\033[48;2;241;90;34m", "\033[38;2;255;255;255m"),  # Orange bg, White text
    ("\033[48;2;39;174;96m", "\033[38;2;255;255;255m"),  # Dark Green bg, White text
    ("\033[48;2;142;68;173m", "\033[38;2;255;255;255m"),  # Dark Purple bg, White text
    ("\033[48;2;211;84;0m", "\033[38;2;255;255;255m"),  # Pumpkin bg, White text
    ("\033[48;2;52;73;94m", "\033[38;2;255;255;255m"),  # Midnight Blue bg, White text
]
RESET_COLOR = "\033[0m"

# Logging function with colors
def log_message(index, message):
    bg_color, text_color = COLOR_TABLE[index % len(COLOR_TABLE)]
    current_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
    print(f'[{current_time}] {appname}: {bg_color}{text_color}{message}{RESET_COLOR}', flush=True)

# Get index
if len(sys.argv) >= 2:
    try:
        index = int(sys.argv[1])
    except ValueError:
        index = 1
log_message(index, f'Index set to {index}')

# Get role
if len(sys.argv) >= 3:
    try:
        role = sys.argv[2]
    except ValueError:
        role = "crash"
log_message(index, f'Role set to {role}')

# Get progress interval (optional 3rd argument)
progress_interval = 0
if len(sys.argv) >= 4:
    try:
        progress_interval = int(sys.argv[3])
    except ValueError:
        progress_interval = 0

# Load configuration after getting role/index
config = configparser.ConfigParser()
config.optionxform = str
config.read('config.ini')
# Read UDP port
UDP_PORT = config.getint('processWatchdog', 'udp_port')
log_message(index, f'UDP_PORT set to {UDP_PORT}')
# Determine the application name based on index
app_names = []
for section in config.sections():
    if section.startswith('app:'):
        app_names.append(section[len('app:'):])

if index - 1 < len(app_names):
    name = app_names[index - 1]
else:
    name = f'Process{index}'

log_message(index, f'Name set to {name}')

if progress_interval > 0:
    log_message(index, f'Progress interval: {progress_interval}s (CLI override)')

# Get the PID
pid = str(os.getpid())
log_message(index, f'Pid is {pid}')

# Read parameters from the specific app section
app_section = f'app:{name}'
if config.has_section(app_section):
    heartbeat_delay = config.getint(app_section, 'heartbeat_delay')
    log_message(index, f'{name} Heartbeat delay set to {heartbeat_delay}')
    heartbeat_interval = config.getint(app_section, 'heartbeat_interval')
    log_message(index, f'{name} Heartbeat interval set to {heartbeat_interval}')
else:
    log_message(index, f'Error: Section {app_section} not found in config.ini. Using default values.')
    heartbeat_delay = 0
    heartbeat_interval = 0

# Set the allowed running time dynamically
max_runtime = max(abs(heartbeat_interval) * 5, 180)
log_message(index, f'{name} Max runtime set to {max_runtime} seconds')

# If heartbeat_interval is zero or negative, do not send periodic heartbeats
if heartbeat_interval <= 0:
    log_message(index, f'{name} No periodic heartbeats will be sent')
    heartbeat_enabled = False
else:
    heartbeat_enabled = True

# Wait during given heartbeat delay if greater than zero
if heartbeat_delay > 0:
    log_message(index, f'{name} Waiting {heartbeat_delay} seconds heartbeat_delay...')
    time.sleep(heartbeat_delay - 1)

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

start_time = time.time()
last_progress_time = start_time

# Calculate initial wait_time based on heartbeat_interval if heartbeat enabled
wait_time = max(heartbeat_delay, 0)
if heartbeat_enabled:
    wait_time = max(random.randint(int(heartbeat_interval / 3), int(heartbeat_interval / 2)), 0)

while True:
    current_time = time.time()
    
    # Send heartbeat (unless disabled) + progress signal (if enabled)
    if heartbeat_enabled:
        data = 'p' + pid
        log_message(index, f'{name} Heartbeat sent: {data} after {wait_time} seconds')
    else:
        data = 'p' + pid

    # Send progress signal if enabled and interval has passed
    send_progress = (progress_interval > 0 and (current_time - last_progress_time) >= progress_interval)
    if send_progress:
        data += '+'
        last_progress_time = current_time
        log_message(index, f'{name} Progress signal sent: {data}')

    # Send the data
    sock.sendto(data.encode('utf-8'), ('localhost', UDP_PORT))

    # Wait for the next event
    if heartbeat_enabled:
        wait_time = max(random.randint(int(heartbeat_interval / 3), int(heartbeat_interval / 2)), 0)
        if wait_time > 0:
            time.sleep(wait_time)
    elif progress_interval > 0:
        # Sleep until next progress signal is due
        time.sleep(progress_interval)
    else:
        # No heartbeat and no progress - just sleep
        time.sleep(1)

    # Check the running time of the process
    elapsed_time = time.time() - start_time
    if elapsed_time >= max_runtime:
        # Send final progress signal before exiting (if enabled)
        if progress_interval > 0:
            data = 'p' + pid + '+'
            sock.sendto(data.encode('utf-8'), ('localhost', UDP_PORT))
            log_message(index, f'{name} Final progress signal before exit: {data}')
            
        if role == "noheartbeat":
            sleep_time = max(abs(heartbeat_interval) * 5, 100)
            interval = max(abs(heartbeat_interval + 1) / 10, 2)
            for _ in range(int(sleep_time / interval)):
                log_message(index, f'{name} No more heartbeats')
                time.sleep(interval)
        elif role == "crash":
            log_message(index, f'{name} process crashed')
            sys.exit(1)
