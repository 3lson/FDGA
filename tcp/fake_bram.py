import socket
import threading
import struct

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
        while True:
            header = conn.recv(4)
            if not header:
                break
            cmd_type, data_len = struct.unpack('!HH', header)

            if cmd_type == 0x0001:  # write
                data = b''
                while len(data) < data_len:
                    chunk = conn.recv(min(4096, data_len - len(data)))
                    data += chunk

                offset = 0
                while offset < data_len:
                    aligned_offset = offset & 0xFFFFFFFC
                    current_val = mmio.read(aligned_offset)
                    current_bytes = bytearray(current_val.to_bytes(4, 'little'))
                    for i in range(min(4, data_len - offset)):
                        current_bytes[(offset % 4) + i] = data[offset + i]
                    new_val = int.from_bytes(current_bytes, 'little')
                    mmio.write(aligned_offset, new_val)
                    offset += 4

                conn.sendall(struct.pack('!I', 0x1))

            elif cmd_type == 0x0002:  # read
                data = bytearray()
                for offset in range(0, data_len, 4):
                    read_size = min(4, data_len - offset)
                    value = mmio.read(offset)
                    data.extend(value.to_bytes(4, 'little')[:read_size])
                header = struct.pack('!II', 0x2, len(data))
                conn.sendall(header + data)

            elif cmd_type == 0x0000:  # handshake
                conn.sendall(struct.pack('!I', 0x0))
            else:
                print(f"Unknown command: {cmd_type}")
                conn.sendall(struct.pack('!I', 0xFFFFFFFF))

    except Exception as e:
        print(f"Error: {e}")
    finally:
        conn.close()


def run_server():
    HOST = '192.168.2.2'
    PORT = 9090

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen(5)
        print(f"Server listening on {HOST}:{PORT}")
        while True:
            conn, addr = s.accept()
            thread = threading.Thread(target=handle_client, args=(conn, addr))
            thread.start()

# Use this inside your Jupyter notebook cell:
# threading.Thread(target=run_server, daemon=True).start()
# print("Fake MMIO server started")
