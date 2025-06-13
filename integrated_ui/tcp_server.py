import socket
import threading
import os
import subprocess

def handle_client(conn, addr):
    print(f'Connected by {addr}')
    try:
        # Wait for START command
        cmd = recv_line(conn)
        if cmd != "START":
            print("Expected START")
            return

        # Define the expected files to receive
        save_targets = ["out/clicked_points.txt", "out/points.txt"]
        for expected_name in save_targets:
            received_filename = recv_line(conn)  # actual filename sent by client
            size = int(recv_line(conn))
            data = recv_exact(conn, size)

            with open(expected_name, "wb") as f:
                f.write(data)

            print(f"Saved {received_filename} as {expected_name} ({size} bytes)")

        # Wait for END
        end_cmd = recv_line(conn)
        if end_cmd != "END":
            print("Expected END")
            return

        # Compile and run the C code
        subprocess.run(['gcc', 'k_means3.c', '-o', 'k_means3'], capture_output=True, text=True)
        subprocess.run(['./k_means3'], capture_output=True, text=True)

        # Send back output.txt
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
    HOST = '127.0.0.1'  # PYNQ's IP or localhost
    PORT = 9090
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((HOST, PORT))
        s.listen(1)
        print(f"Server listening on {HOST}:{PORT}")
        while True:
            conn, addr = s.accept()
            threading.Thread(target=handle_client, args=(conn, addr)).start()

if __name__ == "__main__":
    run_server()
