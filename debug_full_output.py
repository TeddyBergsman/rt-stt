#!/usr/bin/env python3
"""
Debug script to show ALL messages from the RT-STT daemon
"""

import json
import socket
import struct
import sys

def connect_and_dump(socket_path="/tmp/rt-stt-test.sock"):
    # Connect
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(socket_path)
    
    # Subscribe to transcriptions
    msg = {
        "type": 1,  # SUBSCRIBE
        "id": "debug-client",
        "data": {}
    }
    
    # Send subscription
    data = json.dumps(msg).encode('utf-8')
    sock.sendall(struct.pack('!I', len(data)))
    sock.sendall(data)
    
    print("Connected and subscribed. Showing ALL messages:")
    print("-" * 50)
    
    # Receive all messages
    try:
        while True:
            # Read length
            length_data = sock.recv(4)
            if not length_data:
                break
            
            length = struct.unpack('!I', length_data)[0]
            
            # Read message
            data = b''
            while len(data) < length:
                chunk = sock.recv(length - len(data))
                if not chunk:
                    break
                data += chunk
            
            # Parse and display
            msg = json.loads(data.decode('utf-8'))
            
            # Pretty print with all details
            print(f"\n[Message Type: {msg['type']}]")
            print(json.dumps(msg, indent=2))
            print("-" * 30)
            
    except KeyboardInterrupt:
        print("\nStopped by user")
    finally:
        sock.close()

if __name__ == "__main__":
    socket_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/rt-stt-test.sock"
    connect_and_dump(socket_path)
