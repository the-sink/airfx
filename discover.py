import socket
import struct

SERVER_IP = socket.gethostbyname("main.local")
SERVER_PORT = 100
COMMAND_DISCOVER = 130

print(SERVER_IP)

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(2.0)

sock.sendto(bytes([COMMAND_DISCOVER]), (SERVER_IP, SERVER_PORT))

try:
    data, addr = sock.recvfrom(1024)
    print(f"Received response from {addr}")

    if data[0] != COMMAND_DISCOVER:
        print("Unexpected response command: ", data[0])
        exit()

    client_count = (len(data) - 1) // 4
    print(f"{client_count} client(s) connected:")

    for i in range(client_count):
        ip_bytes = data[1 + i*4 : 1 + (i+1)*4]
        ip_tuple = struct.unpack(">BBBB", ip_bytes)
        print(" -", ".".join(map(str, ip_tuple)))

except TimeoutError:
    print("No response from server (timeout)")
finally:
    sock.close()