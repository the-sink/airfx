import time
import os
import socket
import struct

SERVER_IP = socket.gethostbyname("main.local")
SERVER_PORT = 100
COMMAND_DOWNLOAD = 131
COMMAND_SYSTEM_START_NOW = 132

def generate_sequence():
    entries = []
    for i in range(256):
        phase = i % 5
        if phase == 0:
            r, g, b = 255, 0, 0
        elif phase == 1:
            r, g, b = 0, 255, 0
        elif phase == 2:
            r, g, b = 0, 0, 255
        elif phase == 3:
            r, g, b = 255, 255, 255
        else:
            r, g, b = 0, 0, 0

        duration = 10 * phase
        packed = struct.pack('<BBBB', r, g, b, duration)
        entries.append(packed)

    return b''.join(entries)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(2.0)

#sock.sendto(bytes([COMMAND_DISCOVER]), (SERVER_IP, SERVER_PORT))

sample_data = [
    [0, 0, generate_sequence()],
    [0, 1, generate_sequence()],
    [0, 2, generate_sequence()],
    [0, 3, generate_sequence()]
]
n = 0

def send_next():
    global n
    #print(n)
    this = sample_data[n]
    n += 1

    checksum = 0
    checksum ^= this[0]
    checksum ^= this[1]
    for b in this[2]:
        checksum ^= b
    
    print("Checksum: ", checksum)
    
    payload = bytearray()
    payload.append(COMMAND_DOWNLOAD)
    payload.append(checksum)
    payload.append(this[0])
    print(this[1])
    payload += this[1].to_bytes(2, 'little')
    payload += this[2]
    
    print("Sending packet")

    sock.sendto(payload, (SERVER_IP, SERVER_PORT))

start_ts = time.time()
send_next()
try:
    while True:
        data, addr = sock.recvfrom(5)
        command = data[0]
        if command != COMMAND_DOWNLOAD:
            print("Unexpected response command:", data[0])
            break
        
        status = data[1]
        device_id = data[2]
        block_index = int.from_bytes(data[3:5], 'little')

        print("Got response with status", status, ", device id", device_id, ", block index", block_index)

        try:
            send_next()
        except IndexError:
            print(time.time() - start_ts)
except TimeoutError: print("Done")

sock.sendto(bytes([COMMAND_SYSTEM_START_NOW]), (SERVER_IP, SERVER_PORT))