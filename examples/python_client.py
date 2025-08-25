#!/usr/bin/env python3
"""
RT-STT Python Client Example
Demonstrates connecting to the RT-STT daemon and receiving transcriptions
"""

import socket
import json
import struct
import threading
import time
import uuid
from typing import Callable, Optional

class RTSTTClient:
    """Simple client for RT-STT daemon"""
    
    def __init__(self, socket_path: str = "/tmp/rt-stt.sock"):
        self.socket_path = socket_path
        self.socket: Optional[socket.socket] = None
        self.connected = False
        self.receive_thread: Optional[threading.Thread] = None
        self._transcription_callback: Optional[Callable[[str], None]] = None
        self._running = False
        
    def connect(self) -> bool:
        """Connect to the RT-STT daemon"""
        try:
            self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self.socket.connect(self.socket_path)
            self.connected = True
            self._running = True
            
            # Start receive thread
            self.receive_thread = threading.Thread(target=self._receive_loop)
            self.receive_thread.daemon = True
            self.receive_thread.start()
            
            print(f"Connected to RT-STT daemon at {self.socket_path}")
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from the daemon"""
        self._running = False
        if self.socket:
            self.socket.close()
        self.connected = False
        if self.receive_thread:
            self.receive_thread.join(timeout=1)
        print("Disconnected from RT-STT daemon")
    
    def _send_message(self, msg_type: int, data: dict, msg_id: str = None) -> bool:
        """Send a message to the daemon"""
        if not self.connected:
            return False
        
        try:
            message = {
                "type": msg_type,
                "id": msg_id or str(uuid.uuid4()),
                "data": data
            }
            
            # Serialize to JSON
            json_data = json.dumps(message)
            json_bytes = json_data.encode('utf-8')
            
            # Send length prefix
            length = struct.pack('!I', len(json_bytes))
            self.socket.sendall(length)
            
            # Send message
            self.socket.sendall(json_bytes)
            return True
        except Exception as e:
            print(f"Failed to send message: {e}")
            return False
    
    def _receive_message(self) -> Optional[dict]:
        """Receive a message from the daemon"""
        try:
            # Read length prefix
            length_data = self.socket.recv(4)
            if not length_data:
                return None
            
            length = struct.unpack('!I', length_data)[0]
            
            # Read message
            data = b''
            while len(data) < length:
                chunk = self.socket.recv(length - len(data))
                if not chunk:
                    return None
                data += chunk
            
            # Parse JSON
            return json.loads(data.decode('utf-8'))
        except Exception as e:
            print(f"Failed to receive message: {e}")
            return None
    
    def _receive_loop(self):
        """Background thread to receive messages"""
        while self._running:
            msg = self._receive_message()
            if not msg:
                break
            
            # Debug: Print raw message
            print(f"[DEBUG] Received message: {msg}")
            
            msg_type = msg.get('type')
            data = msg.get('data', {})
            
            if msg_type == 3:  # TRANSCRIPTION
                text = data.get('text', '')
                confidence = data.get('confidence', 1.0)
                if self._transcription_callback:
                    self._transcription_callback(text)
                else:
                    print(f"Transcription: {text} (confidence: {confidence:.2f})")
            elif msg_type == 4:  # STATUS
                print(f"Status update: {data}")
            elif msg_type == 5:  # ERROR
                print(f"Error: {data.get('message', 'Unknown error')}")
            elif msg_type == 6:  # ACKNOWLEDGMENT
                # Only print verbose acknowledgments if they contain an error
                if not data.get('success', True) or 'error' in data:
                    print(f"Command response: {data}")
            else:
                print(f"[DEBUG] Unknown message type {msg_type}: {data}")
    
    def on_transcription(self, callback: Callable[[str], None]):
        """Set callback for transcriptions"""
        self._transcription_callback = callback
    
    def pause(self):
        """Pause listening"""
        return self._send_message(0, {"action": "pause", "params": {}})  # COMMAND
    
    def resume(self):
        """Resume listening"""
        return self._send_message(0, {"action": "resume", "params": {}})  # COMMAND
    
    def get_status(self):
        """Get current status"""
        return self._send_message(0, {"action": "get_status", "params": {}})  # COMMAND
    
    def set_language(self, language: str):
        """Set recognition language"""
        return self._send_message(0, {"action": "set_language", "params": {"language": language}})  # COMMAND


def main():
    """Example usage"""
    import sys
    
    # Allow socket path override from command line
    socket_path = sys.argv[1] if len(sys.argv) > 1 else "/tmp/rt-stt.sock"
    
    client = RTSTTClient(socket_path)
    
    if not client.connect():
        print("Failed to connect to RT-STT daemon")
        print("Is the daemon running? Try: sudo launchctl start com.rt-stt.daemon")
        return
    
    # Set up transcription handler
    def handle_transcription(text: str):
        print(f"\n>>> {text}")
    
    client.on_transcription(handle_transcription)
    
    print("\nListening for transcriptions...")
    print("Commands:")
    print("  p - Pause listening")
    print("  r - Resume listening")
    print("  s - Get status")
    print("  q - Quit")
    print()
    
    try:
        while True:
            cmd = input().strip().lower()
            
            if cmd == 'p':
                client.pause()
                print("Paused")
            elif cmd == 'r':
                client.resume()
                print("Resumed")
            elif cmd == 's':
                client.get_status()
            elif cmd == 'q':
                break
            else:
                print("Unknown command")
    except KeyboardInterrupt:
        print("\nInterrupted")
    
    client.disconnect()


if __name__ == "__main__":
    main()
