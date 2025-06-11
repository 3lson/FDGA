import socket
import struct

HOST = '192.168.2.2'
PORT = 9090

class PynqClient:
    def __init__(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((HOST, PORT))
    
    def send_data(self, data):
        #send write command header
        header = struct.pack('!HH', 0x0001, len(data))
        self.sock.sendall(header)
        
        #send data in packets
        self.sock.sendall(data)
        
        #wait for ack
        ack = self.sock.recv(4)
        return struct.unpack('!I', ack)[0] == 0x1
    
    def read_data(self, length):
        #send read command header
        header = struct.pack('!HH', 0x0002, length)
        self.sock.sendall(header)
        
        #read response header
        resp_header = self.sock.recv(8)
        _, size = struct.unpack('!II', resp_header)
        
        #read data in packets
        data = b''
        while len(data) < size:
            chunk = self.sock.recv(min(4096, size - len(data)))
            if not chunk:
                break
            data += chunk
        return data
    
    # test connection
    def handshake(self):
        self.sock.sendall(struct.pack('!HH', 0x0000, 0))
        return struct.unpack('!I', self.sock.recv(4))[0] == 0x0
    
    def close(self):
        self.sock.close()


if __name__ == "__main__":
    client = PynqClient()
    
    #test connection
    print("Handshake response:", client.handshake())

    
    #read data from BRAM
    read_back = client.read_data(1024)  #read 1KB
    client.close()
