import socket
import struct
import json
import os

HOST = '192.168.2.2'
PORT = 9090

class PynqClient:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((HOST, PORT))
    
    def send_data(self, data):
        header = struct.pack('!HH', 0x0001, len(data))
        self.sock.sendall(header)
        self.sock.sendall(data)
        ack = self.sock.recv(4)
        return struct.unpack('!I', ack)[0] == 0x1
    
    def read_data(self, length):
        header = struct.pack('!HH', 0x0002, length)
        self.sock.sendall(header)
        resp_header = self.sock.recv(8)
        _, size = struct.unpack('!II', resp_header)
        data = b''
        while len(data) < size:
            chunk = self.sock.recv(min(4096, size - len(data)))
            if not chunk:
                break
            data += chunk
        return data
    
    def handshake(self):
        self.sock.sendall(struct.pack('!HH', 0x0000, 0))
        return struct.unpack('!I', self.sock.recv(4))[0] == 0x0
    
    def close(self):
        self.sock.close()


def test_handshake():
    client = PynqClient()
    assert client.handshake(), "Handshake failed"
    print("✅ Handshake test passed")
    client.close()


def test_raw_data():
    client = PynqClient()
    client.handshake()
    test_data = os.urandom(128)
    assert client.send_data(test_data), "Write failed"
    read_back = client.read_data(128)
    assert test_data == read_back, "Data mismatch"
    print("✅ Raw data test passed")
    client.close()


def test_text_file():
    test_text = "Hello, this is a test string.\nIt spans multiple lines.\nEnd of file.\n"
    with open("sample.txt", "w") as f:
        f.write(test_text)

    with open("sample.txt", "rb") as f:
        file_data = f.read()

    client = PynqClient()
    client.handshake()
    assert client.send_data(file_data), "Text write failed"
    read_back = client.read_data(len(file_data))

    with open("received_sample.txt", "wb") as f:
        f.write(read_back)

    assert file_data == read_back, "Text file mismatch"
    print("✅ Text file test passed")
    client.close()


def test_json():
    data_dict = {
        "temperature": 25,
        "status": "running",
        "valid": True,
        "sensor": [1, 2, 3, 4]
    }
    json_bytes = json.dumps(data_dict).encode("utf-8")

    client = PynqClient()
    client.handshake()
    assert client.send_data(json_bytes), "JSON write failed"
    read_back = client.read_data(len(json_bytes))
    read_dict = json.loads(read_back.decode("utf-8"))
    assert data_dict == read_dict, "JSON content mismatch"
    print("✅ JSON file test passed")
    client.close()


if __name__ == "__main__":
    print("== PYNQ TCP Test ==")
    test_handshake()
    test_raw_data()
    test_text_file()
    test_json()
    print("🎉 All tests passed successfully")
