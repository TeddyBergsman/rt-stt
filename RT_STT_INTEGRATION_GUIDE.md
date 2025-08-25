# RT-STT Integration Guide

## Overview

RT-STT (Real-Time Speech-to-Text) is a low-latency, offline speech recognition daemon for macOS that runs continuously in the background. It uses Whisper models optimized for Apple Silicon and provides transcriptions via Unix Domain Socket IPC.

## System Requirements

- **macOS**: 11.0 or later
- **Hardware**: Apple Silicon (M1/M2/M3/M4) recommended
- **Memory**: 2-8GB depending on model
- **Storage**: 100MB-6GB for models
- **Audio**: Core Audio compatible device

## Quick Start

```python
# Minimal example - just 5 lines!
from rt_stt import RTSTTClient

with RTSTTClient() as client:
    client.subscribe()
    for transcription in client.transcriptions():
        print(transcription.text)
```

## Installation

### Python Client Library
```bash
pip install /path/to/rt-stt/python
# or
pip3 install --user /path/to/rt-stt/python
```

### Verify Daemon is Running
```bash
# Check if daemon is running
rt-stt-ctl status

# If not running, start it
rt-stt-ctl start

# View logs
rt-stt-ctl logs
```

## Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Microphone    │────▶│   RT-STT Daemon │────▶│ Your Application│
│  (MOTU M2 Ch2)  │     │  (Background)   │     │   (Any Language)│
└─────────────────┘     └─────────────────┘     └─────────────────┘
                              │
                              ▼
                        Unix Domain Socket
                        /tmp/rt-stt.sock
```

## IPC Protocol

### Connection
- **Type**: Unix Domain Socket
- **Path**: `/tmp/rt-stt.sock`
- **Protocol**: Length-prefixed JSON messages
- **Format**: `[4-byte length][JSON payload]`

### Message Structure

#### Client → Server
```json
{
    "command": "subscribe|unsubscribe|pause|resume|get_status|set_language|set_model|...",
    "data": { /* command-specific parameters */ }
}
```

#### Server → Client
```json
{
    "type": 1-8,  // Message type enum
    "data": { /* type-specific data */ }
}
```

### Message Types
1. `ACK` - Command acknowledged
2. `ERROR` - Error occurred
3. `TRANSCRIPTION` - New transcription
4. `STATUS` - Status response
5. `CONFIG` - Configuration data
6. `METRICS` - Performance metrics
7. `MODEL_CHANGED` - Model change notification
8. `LANGUAGE_CHANGED` - Language change notification

## Available Commands

### Basic Control
```json
// Subscribe to transcriptions
{"command": "subscribe"}

// Unsubscribe from transcriptions
{"command": "unsubscribe"}

// Pause transcription
{"command": "pause"}

// Resume transcription
{"command": "resume"}

// Get status
{"command": "get_status"}
```

### Configuration
```json
// Change language (use "auto" for automatic detection)
{"command": "set_language", "data": {"language": "en"}}  // English
{"command": "set_language", "data": {"language": "es"}}  // Spanish
{"command": "set_language", "data": {"language": "fr"}}  // French
{"command": "set_language", "data": {"language": "de"}}  // German
{"command": "set_language", "data": {"language": "auto"}} // Auto-detect

// Change model
{"command": "set_model", "data": {"model_path": "models/ggml-medium.en.bin"}}

// Get current configuration
{"command": "get_config"}

// Set complete configuration
{"command": "set_config", "data": {
    "config": {
        "model_config": { /* see below */ },
        "vad_config": { /* see below */ },
        "capture_config": { /* see below */ }
    },
    "save": true  // Save to disk (optional, default: true)
}}

// Adjust VAD sensitivity (simplified)
{"command": "set_vad_sensitivity", "data": {
    "sensitivity": 1.08  // Single value sets speech_start_threshold
}}

// Get performance metrics
{"command": "get_metrics"}
```

### Monitoring
```json
// Get performance metrics
{"command": "get_metrics"}
```

## Transcription Output Format

When subscribed, you receive transcriptions with full metadata:

```json
{
    "type": 3,
    "data": {
        "text": "Hello world",
        "confidence": 0.95,
        "timestamp": 1693001234567,
        "language": "en",
        "language_probability": 0.99,
        "processing_time_ms": 150,
        "audio_duration_ms": 2000,
        "model": "ggml-small.en.bin",
        "is_final": true,
        "segments": [
            {
                "id": 0,
                "seek": 0,
                "start": 0.0,
                "end": 2.0,
                "text": " Hello world",
                "tokens": [50364, 2425, 1002, 50464],
                "temperature": 0.0,
                "avg_logprob": -0.25,
                "compression_ratio": 1.2,
                "no_speech_prob": 0.01
            }
        ]
    }
}
```

## Client Libraries

### Python Client

```python
from rt_stt import RTSTTClient

# Connect to daemon
client = RTSTTClient("/tmp/rt-stt.sock")
client.connect()

# Subscribe to transcriptions
client.subscribe()

# Handle transcriptions (blocking)
for transcription in client.transcriptions():
    print(f"Text: {transcription.text}")
    print(f"Confidence: {transcription.confidence}")
    print(f"Language: {transcription.language}")
    
    # Access segment details
    for segment in transcription.segments:
        print(f"  Segment: {segment['text']}")
        print(f"  Tokens: {segment['tokens']}")

# Alternative: Use callbacks (non-blocking)
def on_transcription(result):
    print(f"Got: {result.text}")

def on_error(error):
    print(f"Error: {error}")

def on_connection(connected):
    print(f"Connected: {connected}")

client.on_transcription(on_transcription)
client.on_error(on_error)
client.on_connection(on_connection)
client.start_listening()  # Non-blocking

# Generator method (blocking) - automatically handles reconnection
for transcription in client.transcriptions():
    # This will block and yield transcriptions as they arrive
    # Automatically reconnects if connection is lost
    print(transcription.text)

# Control commands
client.pause()           # Pause listening
client.resume()          # Resume listening
client.unsubscribe()     # Stop receiving transcriptions

# Configuration commands
client.set_language("auto")         # Auto-detect language
client.set_language("es")           # Spanish
client.set_model("models/ggml-large-v3.bin")  # Change model
client.set_vad_sensitivity(1.08)    # Adjust VAD

# Advanced configuration
client.set_vad_config(
    speech_start_threshold=1.08,
    speech_end_threshold=0.85,
    min_speech_ms=100,
    pre_speech_buffer_ms=500
)

client.set_model_config(
    model_path="models/ggml-medium.en.bin",
    language="en",
    temperature=0.0,
    beam_size=5
)

# Get information
status = client.get_status()
print(f"Listening: {status.listening}")
print(f"Model: {status.model}")

config = client.get_config()
metrics = client.get_metrics()

# Context manager usage
with RTSTTClient() as client:
    client.subscribe()
    # Automatically disconnects on exit

client.disconnect()
```

### Raw Socket (Any Language)

Example in Python showing the raw protocol:

```python
import socket
import struct
import json

# Connect
sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.connect("/tmp/rt-stt.sock")

def send_message(sock, message):
    data = json.dumps(message).encode('utf-8')
    length = struct.pack('!I', len(data))
    sock.sendall(length + data)

def receive_message(sock):
    # Read length
    length_data = sock.recv(4)
    if not length_data:
        return None
    length = struct.unpack('!I', length_data)[0]
    
    # Read data
    data = b''
    while len(data) < length:
        chunk = sock.recv(min(length - len(data), 4096))
        if not chunk:
            return None
        data += chunk
    
    return json.loads(data.decode('utf-8'))

# Subscribe
send_message(sock, {"command": "subscribe"})

# Receive transcriptions
while True:
    msg = receive_message(sock)
    if msg and msg['type'] == 3:  # TRANSCRIPTION
        print(f"Got: {msg['data']['text']}")
```

### C++ Example

```cpp
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

class RTSTTClient {
private:
    int socket_fd;
    
    bool send_message(const nlohmann::json& msg) {
        std::string data = msg.dump();
        uint32_t length = htonl(data.size());
        
        if (send(socket_fd, &length, 4, 0) != 4) return false;
        if (send(socket_fd, data.c_str(), data.size(), 0) != data.size()) return false;
        return true;
    }
    
    nlohmann::json receive_message() {
        uint32_t length;
        if (recv(socket_fd, &length, 4, 0) != 4) return {};
        length = ntohl(length);
        
        std::vector<char> buffer(length);
        size_t received = 0;
        while (received < length) {
            ssize_t n = recv(socket_fd, buffer.data() + received, length - received, 0);
            if (n <= 0) return {};
            received += n;
        }
        
        return nlohmann::json::parse(buffer.begin(), buffer.end());
    }

public:
    bool connect(const std::string& socket_path) {
        socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (socket_fd < 0) return false;
        
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
        
        return ::connect(socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0;
    }
    
    void subscribe() {
        send_message({{"command", "subscribe"}});
    }
    
    void listen_for_transcriptions() {
        while (true) {
            auto msg = receive_message();
            if (msg["type"] == 3) { // TRANSCRIPTION
                std::cout << "Text: " << msg["data"]["text"] << std::endl;
                std::cout << "Confidence: " << msg["data"]["confidence"] << std::endl;
            }
        }
    }
};
```

## Integration Patterns

### 1. Simple Transcription Stream
```python
# Minimal integration - just get text
client = RTSTTClient()
client.connect()
client.subscribe()

for t in client.transcriptions():
    process_text(t.text)
```

### 2. Voice Command System
```python
# Use confidence and finality for commands
CONFIDENCE_THRESHOLD = 0.8

for t in client.transcriptions():
    if t.is_final and t.confidence > CONFIDENCE_THRESHOLD:
        if t.text.lower().startswith("computer"):
            execute_command(t.text)
```

### 3. Real-time Translation
```python
# Use language detection
for t in client.transcriptions():
    if t.language != target_language:
        translated = translate(t.text, t.language, target_language)
        display(translated)
```

### 4. Transcription with Timing
```python
# Use segment timing for subtitles
for t in client.transcriptions():
    for segment in t.segments:
        show_subtitle(
            text=segment['text'],
            start_time=segment['start'],
            end_time=segment['end']
        )
```

### 5. Performance Monitoring
```python
# Monitor system health
metrics = client.get_metrics()
if metrics['avg_processing_time_ms'] > 500:
    log_warning("High latency detected")
```

## Configuration Options

### Complete Configuration Format
```json
{
    "model_config": {
        "model_path": "models/ggml-small.en.bin",
        "language": "en",        // or "auto" for detection
        "use_gpu": true,
        "gpu_device": 0,
        "num_threads": 4,
        "max_context": 1500,
        "beam_size": 5,
        "best_of": 5,
        "temperature": 0.0
    },
    "vad_config": {
        "sample_rate": 16000,
        "frame_duration_ms": 30,
        "use_adaptive_threshold": true,
        "energy_threshold": 0.0,
        "speech_start_threshold": 1.08,
        "speech_end_threshold": 0.85,
        "speech_timeout_ms": 300,
        "silence_timeout_ms": 1000,
        "min_speech_duration_ms": 100,
        "max_speech_duration_ms": 30000,
        "pre_speech_buffer_ms": 500,
        "noise_floor_adaptation_rate": 0.01
    },
    "capture_config": {
        "sample_rate": 16000,
        "buffer_duration_ms": 100,
        "device_name": "MOTU M2",
        "force_single_channel": true,
        "input_channel_index": 1    // 0=Input1, 1=Input2
    },
    "ipc_socket_path": "/tmp/rt-stt.sock"
}
```

### Supported Languages

For multilingual models (`ggml-*.bin` without `.en`):
- `"auto"` - Automatic language detection
- `"en"` - English
- `"es"` - Spanish
- `"fr"` - French
- `"de"` - German
- `"it"` - Italian
- `"ja"` - Japanese
- `"ko"` - Korean
- `"pt"` - Portuguese
- `"ru"` - Russian
- `"zh"` - Chinese
- And 90+ more languages

For English-only models (`ggml-*.en.bin`):
- Only `"en"` is supported

## Available Models

| Model | Size | Languages | Speed | Accuracy | Notes |
|-------|------|-----------|-------|----------|-------|
| `ggml-tiny.en.bin` | 39MB | English only | Fastest | Lowest | Testing only |
| `ggml-base.en.bin` | 142MB | English only | Very Fast | Low | Quick drafts |
| `ggml-small.en.bin` | 466MB | English only | Fast | Good | **Default** |
| `ggml-medium.en.bin` | 1.5GB | English only | Medium | Better | Higher accuracy |
| `ggml-tiny.bin` | 39MB | 99 languages | Fast | Lowest | Testing only |
| `ggml-base.bin` | 142MB | 99 languages | Fast | Low | Quick multilingual |
| `ggml-small.bin` | 466MB | 99 languages | Medium | Good | Balanced |
| `ggml-medium.bin` | 1.5GB | 99 languages | Slow | Better | Quality focus |
| `ggml-large-v3.bin` | 3.1GB | 99 languages | Slowest | Best | Maximum accuracy |

## Response Formats

### Status Response
```json
{
    "listening": true,
    "model": "/usr/local/share/rt-stt/models/ggml-small.en.bin",
    "language": "en",
    "vad_enabled": true,
    "clients": 2
}
```

### Metrics Response
```json
{
    "total_transcriptions": 156,
    "total_audio_processed_ms": 234567,
    "avg_processing_time_ms": 187,
    "min_processing_time_ms": 89,
    "max_processing_time_ms": 512,
    "avg_confidence": 0.92,
    "session_start_time": 1693001234567,
    "errors": 0
}
```

### Error Response
```json
{
    "type": 2,  // ERROR
    "data": {
        "error": "Model file not found",
        "code": "MODEL_NOT_FOUND",
        "details": "Path: models/invalid.bin"
    }
}
```

## Error Handling

### Connection Errors
```python
try:
    client.connect()
except ConnectionError:
    # Daemon not running
    print("RT-STT daemon is not running")
    print("Start with: rt-stt-ctl start")
```

### Message Errors
```python
response = client.send_command("set_model", {"model_path": "invalid.bin"})
if response.type == MessageType.ERROR:
    print(f"Error: {response.data['error']}")
```

## Performance Considerations

1. **Latency**: Typically 150-300ms end-to-end
2. **CPU Usage**: ~10-20% during active speech
3. **Memory**: ~500MB-2GB depending on model
4. **Network**: None required (fully offline)

## Troubleshooting

### Daemon Not Running
```bash
# Check status
rt-stt-ctl status

# Start daemon
rt-stt-ctl start

# View logs
rt-stt-ctl logs
```

### No Transcriptions
1. Check microphone permissions
2. Verify correct input channel (Input 2 by default)
3. Check VAD sensitivity settings
4. Ensure model file exists

### High Latency
1. Use smaller model (small.en vs large-v3)
2. Reduce beam_size in config
3. Check CPU throttling
4. Close other intensive applications

## Security Notes

- Socket is user-only accessible (permissions: 0600)
- No network access required or used
- All processing is local
- Audio is not stored or logged

## CLI Tools

### System Control (rt-stt-ctl)
```bash
rt-stt-ctl start      # Start daemon
rt-stt-ctl stop       # Stop daemon  
rt-stt-ctl restart    # Restart daemon
rt-stt-ctl status     # Check if running
rt-stt-ctl logs       # View daemon logs
```

### Client CLI (Python)
```bash
# Stream transcriptions
rt-stt-cli stream
rt-stt-cli stream -j              # JSON output
rt-stt-cli stream -t              # With timestamps
rt-stt-cli stream -c              # With confidence

# Status and control
rt-stt-cli status                 # Get daemon status
rt-stt-cli pause                  # Pause listening
rt-stt-cli resume                 # Resume listening

# Configuration
rt-stt-cli set-language auto      # Auto-detect
rt-stt-cli set-language es        # Spanish
rt-stt-cli set-model models/ggml-large-v3.bin
rt-stt-cli set-vad-sensitivity 1.08

# Advanced configuration
rt-stt-cli get-config             # View config
rt-stt-cli get-config -j          # As JSON
rt-stt-cli set-config '{"model_config":{"language":"auto"}}'
rt-stt-cli set-vad --start-threshold 1.08 --end-threshold 0.85

# Monitoring
rt-stt-cli monitor                # Live dashboard
rt-stt-cli get-metrics            # Performance stats
rt-stt-cli get-metrics -j         # As JSON
```

### C++ CLI (Limited Features)
```bash
rt-stt-cli status                 # Status only
rt-stt-cli stream                 # Stream transcriptions
rt-stt-cli stream -j              # JSON output
```

## Common Integration Patterns

### Shell Script Integration
```bash
#!/bin/bash
# Listen for wake word
rt-stt-cli stream | while read -r line; do
    if [[ "$line" =~ "hey computer" ]]; then
        echo "Wake word detected!"
        # Process command...
    fi
done
```

### JSON Processing with jq
```bash
# Extract just text
rt-stt-cli stream -j | jq -r '.data.text'

# Filter by confidence
rt-stt-cli stream -j | jq -r 'select(.data.confidence > 0.9) | .data.text'

# Get language info
rt-stt-cli stream -j | jq '{text: .data.text, lang: .data.language}'
```

## Example Integration Checklist

When integrating RT-STT:

- [ ] Install client library or implement socket protocol
- [ ] Connect to `/tmp/rt-stt.sock`
- [ ] Subscribe to transcriptions
- [ ] Handle all message types gracefully
- [ ] Implement reconnection logic
- [ ] Set appropriate language/model for use case
- [ ] Monitor performance metrics
- [ ] Handle errors appropriately
- [ ] Test with different audio inputs
- [ ] Verify language detection works (if using auto)

## Support

- Daemon logs: `~/Library/Logs/rt-stt.log`
- Config location: `~/Library/Application Support/rt-stt/config.json`
- Models location: `/usr/local/share/rt-stt/models/`
- Socket path: `/tmp/rt-stt.sock`

## Additional Documentation

- **Complete Reference**: See `RT_STT_COMPLETE_REFERENCE.md` for exhaustive details on all features
- **Troubleshooting**: See `TROUBLESHOOTING_IPC.md` for connection issues
- **Configuration**: See `CONFIG_MANAGEMENT.md` for advanced configuration
- **Metadata**: See `FULL_METADATA.md` for transcription data fields
