#!/usr/bin/env python3
"""Debug IPC connection"""

import socket
import struct
import json
import time

def main():
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect('/tmp/rt-stt-test.sock')
    print("Connected!")
    
    # Send get_status command
    msg = {
        "type": 0,  # COMMAND
        "id": "test_1",
        "data": {
            "action": "get_status",
            "params": {}
        }
    }
    
    json_data = json.dumps(msg).encode('utf-8')
    length = struct.pack('!I', len(json_data))
    
    print(f"Sending {len(json_data)} bytes: {json_data}")
    sock.send(length + json_data)
    
    # Try to receive any data
    print("\nWaiting for data...")
    sock.settimeout(5.0)
    
    try:
        while True:
            # Try to read length prefix
            length_data = sock.recv(4)
            if not length_data:
                print("No data received")
                break
                
            print(f"Received length prefix: {length_data.hex()}")
            length = struct.unpack('!I', length_data)[0]
            print(f"Message length: {length}")
            
            # Read message
            data = sock.recv(length)
            print(f"Received data: {data}")
            
            try:
                msg = json.loads(data)
                print(f"Parsed message: {msg}")
            except Exception as e:
                print(f"Failed to parse: {e}")
                
    except socket.timeout:
        print("Timeout - no data received")
    except Exception as e:
        print(f"Error: {e}")
    
    sock.close()

if __name__ == "__main__":
    main()
