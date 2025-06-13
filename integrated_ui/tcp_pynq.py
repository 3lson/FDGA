import socket
import threading
import os
import subprocess
from pynq import Overlay, MMIO

# FAKE MMIO Simulation using a bytearray (simulates 64KB of BRAM)
class FakeMMIO:
    def __init__(self, size=0x10000):
        self.memory = bytearray(size)

    def read(self, offset):
        # Return 4 bytes as a little-endian integer
        data = self.memory[offset:offset+4]
        return int.from_bytes(data.ljust(4, b'\x00'), 'little')

    def write(self, offset, value):
        data = value.to_bytes(4, 'little')
        self.memory[offset:offset+4] = data

# Use simulated MMIO instead of actual overlay
mmio = FakeMMIO()

def handle_client(conn, addr):
    print(f'Connected by {addr}')
    try:
        # Wait for start command
        cmd = recv_line(conn)
        if cmd != "START":
            print("Expected START")
            return

        #define the expected files to receive
        save_targets = ["out/clicked_points.txt", "out/points.txt"]
        for expected_name in save_targets:
            received_filename = recv_line(conn)  # actual filename sent by client
            size = int(recv_line(conn))
            data = recv_exact(conn, size)

            with open(expected_name, "wb") as f:
                f.write(data)

            print(f"Saved {received_filename} as {expected_name} ({size} bytes)")

        #wait for END
        end_cmd = recv_line(conn)
        if end_cmd != "END":
            print("Expected END")
            return

        #compile and run the C code
        subprocess.run(['gcc', 'k_means3.c', '-o', 'k_means3'], capture_output=True, text=True)
        subprocess.run(['./k_means3'], capture_output=True, text=True)

        #send back output.txt
        if not os.path.exists("out/output.txt"):
            print("output.txt not generated.")
            return

        with open("out/output.txt", "rb") as f:
            output_data = f.read()

        conn.sendall(b"output.txt\n")
        conn.sendall(f"{len(output_data)}\n".encode())
        conn.sendall(output_data)
        print("Sent output.txt")

    except Exception as e:
        print(f"Error: {e}")
    finally:
        conn.close()

def recv_line(sock):
    data = b""
    while not data.endswith(b"\n"):
        part = sock.recv(1)
        if not part:
            break
        data += part
    return data.decode().strip()

def recv_exact(sock, size):
    data = b""
    while len(data) < size:
        part = sock.recv(min(4096, size - len(data)))
        if not part:
            break
        data += part
    return data

def run_server():
    HOST = '192.168.2.2'  #PYNQ IP change as needed
    PORT = 9090
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen(1)
        print(f"Server listening on {HOST}:{PORT}")
        while True:
            conn, addr = s.accept()
            thread = threading.Thread(target=handle_client, args=(conn, addr))
            thread.start()

import threading
threading.Thread(target=run_server, daemon=True).start()
print("TCP server started")
