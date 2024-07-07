# processWatchdog application test child python script
# Copyright (c) 2023 Eray Ozturk <erayozturk1@gmail.com>

# Usage:
# python test_child <index> <role>
# python test_child 1 crash
# python test_child 2 noping

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

# Get the current timestamp
current_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())

# Print a message to stdout
print(f'[{current_time}] {appname}: Application started', flush=True)

# Get index
if len(sys.argv) >= 2:
    try:
        index = int(sys.argv[1])
    except ValueError:
        index = 1
print(f'[{current_time}] {appname}: Index set to {index}')

# Get role
if len(sys.argv) >= 3:
    try:
        role = sys.argv[2]
    except ValueError:
        role = "crash"
print(f'[{current_time}] {appname}: Role set to {role}')

# Store the parameters in variables
config = configparser.ConfigParser()
config.optionxform=str
config.read('config.ini')
UDP_PORT = config.getint('processWatchdog', 'udp_port')
print(f'[{current_time}] {appname}: UDP_PORT set to {UDP_PORT}')

#start_delay = config.getint('processWatchdog', str(index) + '_start_delay')
#print(f'[{current_time}] {appname}: Start delay set to {start_delay}')

ping_delay = config.getint('processWatchdog', str(index) + '_ping_delay')
print(f'[{current_time}] {appname}: Ping delay set to {ping_delay}')

ping_interval = config.getint('processWatchdog', str(index) + '_ping_interval')
print(f'[{current_time}] {appname}: Ping interval set to {ping_interval}')

name = config.get('processWatchdog', str(index) + '_name')
print(f'[{current_time}] {appname}: Name set to {name}')

# Create a UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Reuse the address
#sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

# Bind the UDP socket
#sock.bind(('localhost', UDP_PORT))

# Get the PID
pid = str(os.getpid())

# Wait during given ping delay
ping_delay = int(ping_delay / 2 + 1)
print(f'[{current_time}] {appname}: Waiting {ping_delay} seconds ping_delay...')
time.sleep(ping_delay)
wait_time = ping_delay

# Set the running time of the program
start_time = time.time()

while True:
    # Create the data
    data = 'p' + pid

    # Send the data
    sock.sendto(data.encode('utf-8'), ('localhost', UDP_PORT))

    # Get the current timestamp
    current_time = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())

    # Print a message to stdout
    print(f'[{current_time}] {appname}: {name} Ping sent: {data} after {wait_time} seconds', flush=True)

    # Choose a random waiting time
    wait_time = random.randint(int(ping_interval / 3), int(ping_interval / 2))

    # Wait for the random time
    time.sleep(wait_time)

    # Check the running time of the program
    elapsed_time = time.time() - start_time
    if elapsed_time >= 180:
        # Print the final message to stdout
        if role == "noping":
            print(f'[{current_time}] {appname}: {name} No more pings', flush=True)
            time.sleep(9000)
        elif role == "crash":
            print(f'[{current_time}] {appname}: {name} Program stopped', flush=True)
            sys.exit(0)
