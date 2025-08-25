# RT-STT IPC Protocol Documentation

## Overview

The RT-STT daemon uses Unix domain sockets for inter-process communication, allowing multiple clients to connect and receive real-time transcriptions.

## Connection Details

- **Socket Path**: `/tmp/rt-stt.sock` (default)
- **Protocol**: JSON over Unix domain socket
- **Message Framing**: 4-byte length prefix (network byte order) + JSON payload

## Message Format

All messages follow this structure:
```json
{
  "type": <message_type_int>,
  "id": "<unique_message_id>",
  "data": { ... }
}
```

## Message Types

### Client → Server

#### 1. COMMAND (type: 0)
Execute a command on the server.

```json
{
  "type": 0,
  "id": "msg_123",
  "data": {
    "action": "pause|resume|get_status|set_language|set_model|set_vad_sensitivity",
    "params": { ... }
  }
}
```

**Supported Actions:**
- `pause`: Stop listening for audio
- `resume`: Start listening for audio
- `get_status`: Get current daemon status
- `set_language`: Change recognition language (params: `{"language": "en"}`)
- `set_model`: Change Whisper model (params: `{"model": "small.en"}`)
- `set_vad_sensitivity`: Adjust VAD sensitivity (params: `{"sensitivity": 1.08}`)

#### 2. SUBSCRIBE (type: 1)
Subscribe to transcription events.

```json
{
  "type": 1,
  "id": "msg_124",
  "data": {}
}
```

#### 3. UNSUBSCRIBE (type: 2)
Unsubscribe from transcription events.

```json
{
  "type": 2,
  "id": "msg_125",
  "data": {}
}
```

### Server → Client

#### 4. TRANSCRIPTION (type: 4)
Real-time transcription result.

```json
{
  "type": 4,
  "id": "msg_126",
  "data": {
    "text": "Hello world",
    "confidence": 0.95,
    "timestamp": 1699123456789
  }
}
```

#### 5. STATUS (type: 5)
Status update from server.

```json
{
  "type": 5,
  "id": "msg_127",
  "data": {
    "listening": true,
    "clients": 2,
    "uptime": 3600
  }
}
```

#### 6. ERROR (type: 6)
Error response.

```json
{
  "type": 6,
  "id": "msg_128",
  "data": {
    "message": "Unknown command"
  }
}
```

#### 7. ACKNOWLEDGMENT (type: 7)
Command acknowledgment.

```json
{
  "type": 7,
  "id": "msg_129",
  "data": {
    "success": true,
    "result": { ... }
  }
}
```

## Python Client Example

```python
import socket
import json
import struct

# Connect
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect('/tmp/rt-stt.sock')

# Send command
cmd = {
    "type": 0,
    "id": "cmd_1",
    "data": {
        "action": "get_status",
        "params": {}
    }
}

# Serialize and send
msg = json.dumps(cmd).encode()
sock.send(struct.pack('!I', len(msg)) + msg)

# Receive response
length = struct.unpack('!I', sock.recv(4))[0]
response = json.loads(sock.recv(length))
print(response)
```

## Node.js Client Example

```javascript
const net = require('net');

const client = net.createConnection('/tmp/rt-stt.sock');

client.on('connect', () => {
    // Send subscribe message
    const msg = JSON.stringify({
        type: 1,
        id: 'sub_1',
        data: {}
    });
    
    // Send with length prefix
    const buffer = Buffer.concat([
        Buffer.allocUnsafe(4),
        Buffer.from(msg)
    ]);
    buffer.writeUInt32BE(msg.length, 0);
    client.write(buffer);
});

// Handle incoming messages
let buffer = Buffer.alloc(0);

client.on('data', (data) => {
    buffer = Buffer.concat([buffer, data]);
    
    while (buffer.length >= 4) {
        const length = buffer.readUInt32BE(0);
        
        if (buffer.length >= 4 + length) {
            const msg = JSON.parse(buffer.toString('utf8', 4, 4 + length));
            
            if (msg.type === 4) { // Transcription
                console.log('Transcribed:', msg.data.text);
            }
            
            buffer = buffer.slice(4 + length);
        } else {
            break;
        }
    }
});
```

## Connection Flow

1. Client connects to Unix socket
2. Server sends STATUS message with capabilities
3. Client is automatically subscribed to transcriptions
4. Client can send COMMAND messages
5. Server broadcasts TRANSCRIPTION messages to all subscribed clients
6. Client can UNSUBSCRIBE if needed
7. Server cleans up on client disconnect

## Error Handling

- Invalid JSON: Connection closed
- Unknown message type: ERROR response
- Command failure: ERROR response with details
- Connection lost: Automatic cleanup
