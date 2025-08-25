"""
RT-STT Client Implementation
"""

import socket
import struct
import json
import threading
import time
import uuid
from typing import Callable, Optional, Dict, Any, List
from dataclasses import dataclass
from enum import IntEnum
import logging

logger = logging.getLogger(__name__)


class MessageType(IntEnum):
    """IPC message types"""
    COMMAND = 0
    SUBSCRIBE = 1
    UNSUBSCRIBE = 2
    TRANSCRIPTION = 3
    STATUS = 4
    ERROR = 5
    ACKNOWLEDGMENT = 6


class RTSTTError(Exception):
    """Base exception for RT-STT client"""
    pass


class ConnectionError(RTSTTError):
    """Connection-related errors"""
    pass


class CommandError(RTSTTError):
    """Command execution errors"""
    pass


@dataclass
class TranscriptionResult:
    """Transcription result from STT engine"""
    text: str
    confidence: float
    timestamp: int
    
    @classmethod
    def from_dict(cls, data: dict) -> 'TranscriptionResult':
        return cls(
            text=data.get('text', ''),
            confidence=data.get('confidence', 1.0),
            timestamp=data.get('timestamp', 0)
        )


@dataclass
class Status:
    """Daemon status information"""
    listening: bool
    model: str
    language: str
    vad_enabled: bool
    clients: int = 0
    uptime: int = 0
    
    @classmethod
    def from_dict(cls, data: dict) -> 'Status':
        return cls(
            listening=data.get('listening', False),
            model=data.get('model', ''),
            language=data.get('language', ''),
            vad_enabled=data.get('vad_enabled', True),
            clients=data.get('clients', 0),
            uptime=data.get('uptime', 0)
        )


class RTSTTClient:
    """
    RT-STT Client for connecting to the daemon
    
    Example:
        client = RTSTTClient()
        client.connect()
        
        def on_transcription(result: TranscriptionResult):
            print(f"Transcribed: {result.text}")
        
        client.on_transcription(on_transcription)
        client.start_listening()
    """
    
    def __init__(self, socket_path: str = "/tmp/rt-stt.sock", 
                 auto_reconnect: bool = True,
                 reconnect_delay: float = 1.0):
        """
        Initialize RT-STT client
        
        Args:
            socket_path: Path to Unix domain socket
            auto_reconnect: Automatically reconnect on disconnect
            reconnect_delay: Delay between reconnection attempts
        """
        self.socket_path = socket_path
        self.auto_reconnect = auto_reconnect
        self.reconnect_delay = reconnect_delay
        
        self._socket: Optional[socket.socket] = None
        self._connected = False
        self._running = False
        self._receive_thread: Optional[threading.Thread] = None
        self._subscribed = False
        
        # Callbacks
        self._transcription_callback: Optional[Callable[[TranscriptionResult], None]] = None
        self._status_callback: Optional[Callable[[Status], None]] = None
        self._error_callback: Optional[Callable[[str], None]] = None
        self._connection_callback: Optional[Callable[[bool], None]] = None
        
        # Response handling
        self._pending_commands: Dict[str, threading.Event] = {}
        self._command_responses: Dict[str, dict] = {}
        self._lock = threading.Lock()
    
    def connect(self) -> bool:
        """
        Connect to the RT-STT daemon
        
        Returns:
            True if connected successfully
        
        Raises:
            ConnectionError: If connection fails
        """
        if self._connected:
            return True
        
        try:
            self._socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self._socket.connect(self.socket_path)
            self._connected = True
            self._running = True
            
            # Start receive thread
            self._receive_thread = threading.Thread(target=self._receive_loop, daemon=True)
            self._receive_thread.start()
            
            # Notify connection callback
            if self._connection_callback:
                self._connection_callback(True)
            
            logger.info(f"Connected to RT-STT daemon at {self.socket_path}")
            
            # Auto-subscribe if we have a transcription callback
            if self._transcription_callback and not self._subscribed:
                self.subscribe()
            
            return True
            
        except Exception as e:
            self._connected = False
            if self._socket:
                self._socket.close()
                self._socket = None
            
            error_msg = f"Failed to connect to {self.socket_path}: {e}"
            logger.error(error_msg)
            raise ConnectionError(error_msg) from e
    
    def disconnect(self):
        """Disconnect from the daemon"""
        self._running = False
        self._connected = False
        
        if self._socket:
            try:
                self._socket.close()
            except:
                pass
            self._socket = None
        
        if self._receive_thread and self._receive_thread.is_alive():
            self._receive_thread.join(timeout=1.0)
        
        # Notify connection callback
        if self._connection_callback:
            self._connection_callback(False)
        
        logger.info("Disconnected from RT-STT daemon")
    
    def _send_message(self, msg_type: MessageType, data: dict, msg_id: str = None) -> str:
        """Send a message to the daemon"""
        if not self._connected:
            raise ConnectionError("Not connected to daemon")
        
        msg_id = msg_id or str(uuid.uuid4())
        
        message = {
            "type": int(msg_type),
            "id": msg_id,
            "data": data
        }
        
        try:
            # Serialize to JSON
            json_data = json.dumps(message).encode('utf-8')
            
            # Send length prefix
            length = struct.pack('!I', len(json_data))
            self._socket.sendall(length)
            
            # Send message
            self._socket.sendall(json_data)
            
            return msg_id
            
        except Exception as e:
            self._handle_disconnect()
            raise ConnectionError(f"Failed to send message: {e}") from e
    
    def _receive_message(self) -> Optional[dict]:
        """Receive a message from the daemon"""
        try:
            # Read length prefix
            length_data = self._socket.recv(4)
            if not length_data:
                return None
            
            length = struct.unpack('!I', length_data)[0]
            
            # Read message
            data = b''
            while len(data) < length:
                chunk = self._socket.recv(length - len(data))
                if not chunk:
                    return None
                data += chunk
            
            # Parse JSON
            return json.loads(data.decode('utf-8'))
            
        except Exception as e:
            logger.error(f"Failed to receive message: {e}")
            return None
    
    def _receive_loop(self):
        """Background thread to receive messages"""
        while self._running:
            msg = self._receive_message()
            if not msg:
                self._handle_disconnect()
                break
            
            try:
                self._process_message(msg)
            except Exception as e:
                logger.error(f"Error processing message: {e}")
    
    def _process_message(self, msg: dict):
        """Process received message"""
        msg_type = MessageType(msg.get('type', -1))
        msg_id = msg.get('id', '')
        data = msg.get('data', {})
        
        if msg_type == MessageType.TRANSCRIPTION:
            if self._transcription_callback:
                result = TranscriptionResult.from_dict(data)
                self._transcription_callback(result)
        
        elif msg_type == MessageType.STATUS:
            if self._status_callback:
                status = Status.from_dict(data)
                self._status_callback(status)
        
        elif msg_type == MessageType.ERROR:
            error_msg = data.get('message', 'Unknown error')
            logger.error(f"Server error: {error_msg}")
            if self._error_callback:
                self._error_callback(error_msg)
        
        elif msg_type == MessageType.ACKNOWLEDGMENT:
            # Handle command responses
            with self._lock:
                if msg_id in self._pending_commands:
                    self._command_responses[msg_id] = data
                    self._pending_commands[msg_id].set()
    
    def _handle_disconnect(self):
        """Handle disconnection from daemon"""
        self._connected = False
        self._subscribed = False
        
        if self._connection_callback:
            self._connection_callback(False)
        
        if self.auto_reconnect and self._running:
            logger.info("Attempting to reconnect...")
            threading.Thread(target=self._reconnect_loop, daemon=True).start()
    
    def _reconnect_loop(self):
        """Attempt to reconnect to daemon"""
        while self._running and not self._connected:
            try:
                time.sleep(self.reconnect_delay)
                self.connect()
                break
            except ConnectionError:
                continue
    
    def _send_command(self, action: str, params: dict = None, timeout: float = 5.0) -> dict:
        """Send command and wait for response"""
        msg_id = self._send_message(
            MessageType.COMMAND,
            {"action": action, "params": params or {}},
        )
        
        # Set up response tracking
        with self._lock:
            event = threading.Event()
            self._pending_commands[msg_id] = event
        
        # Wait for response
        if not event.wait(timeout):
            with self._lock:
                self._pending_commands.pop(msg_id, None)
            raise CommandError(f"Command '{action}' timed out")
        
        # Get response
        with self._lock:
            response = self._command_responses.pop(msg_id, {})
            self._pending_commands.pop(msg_id, None)
        
        if not response.get('success', False):
            raise CommandError(f"Command '{action}' failed: {response}")
        
        return response.get('result', {})
    
    # Public API methods
    
    def subscribe(self):
        """Subscribe to transcription events"""
        self._send_message(MessageType.SUBSCRIBE, {})
        self._subscribed = True
        logger.info("Subscribed to transcriptions")
    
    def unsubscribe(self):
        """Unsubscribe from transcription events"""
        self._send_message(MessageType.UNSUBSCRIBE, {})
        self._subscribed = False
        logger.info("Unsubscribed from transcriptions")
    
    def pause(self):
        """Pause listening"""
        return self._send_command("pause")
    
    def resume(self):
        """Resume listening"""
        return self._send_command("resume")
    
    def get_status(self) -> Status:
        """Get current daemon status"""
        result = self._send_command("get_status")
        return Status.from_dict(result)
    
    def set_language(self, language: str):
        """Set recognition language"""
        return self._send_command("set_language", {"language": language})
    
    def set_model(self, model: str):
        """Set Whisper model"""
        return self._send_command("set_model", {"model": model})
    
    def set_vad_sensitivity(self, sensitivity: float):
        """Set VAD sensitivity (e.g., 1.08)"""
        return self._send_command("set_vad_sensitivity", {"sensitivity": sensitivity})
    
    # Callback setters
    
    def on_transcription(self, callback: Callable[[TranscriptionResult], None]):
        """Set callback for transcription results"""
        self._transcription_callback = callback
        if self._connected and not self._subscribed:
            self.subscribe()
    
    def on_status(self, callback: Callable[[Status], None]):
        """Set callback for status updates"""
        self._status_callback = callback
    
    def on_error(self, callback: Callable[[str], None]):
        """Set callback for errors"""
        self._error_callback = callback
    
    def on_connection(self, callback: Callable[[bool], None]):
        """Set callback for connection state changes"""
        self._connection_callback = callback
    
    # Context manager support
    
    def __enter__(self):
        self.connect()
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()
    
    # Convenience methods
    
    def start_listening(self):
        """Start listening (resume + subscribe)"""
        if not self._subscribed:
            self.subscribe()
        self.resume()
    
    def stop_listening(self):
        """Stop listening (pause)"""
        self.pause()
    
    @property
    def is_connected(self) -> bool:
        """Check if connected to daemon"""
        return self._connected
