# RT-STT IPC Server Implementation

## Overview

The RT-STT daemon now includes a full IPC (Inter-Process Communication) server that allows other applications to:
- Receive real-time speech transcriptions
- Control the STT engine (pause/resume)
- Query status and configuration
- Change settings on the fly

## Quick Start

### 1. Start the RT-STT Daemon

```bash
# Test mode (foreground)
./test_ipc.sh

# Or run directly
./build/rt-stt --socket /tmp/rt-stt.sock
```

### 2. Connect with Python Client

In another terminal:

```bash
./test_python_client.sh
```

Or use the Python client directly:

```python
from examples.python_client import RTSTTClient

client = RTSTTClient()
client.connect()
client.on_transcription(lambda text: print(f">>> {text}"))

# Keep running to receive transcriptions
input("Press Enter to quit...")
client.disconnect()
```

## Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Your App      │     │   Your App      │     │   Your App      │
│   (Python)      │     │   (Node.js)     │     │   (Any Lang)    │
└────────┬────────┘     └────────┬────────┘     └────────┬────────┘
         │                       │                       │
         └───────────────────────┴───────────────────────┘
                                 │
                         Unix Domain Socket
                         /tmp/rt-stt.sock
                                 │
                    ┌────────────┴────────────┐
                    │     RT-STT Daemon       │
                    │                         │
                    │  ┌─────────────────┐   │
                    │  │   IPC Server    │   │
                    │  └────────┬────────┘   │
                    │           │            │
                    │  ┌────────┴────────┐   │
                    │  │   STT Engine    │   │
                    │  │  (Whisper.cpp)  │   │
                    │  └─────────────────┘   │
                    └─────────────────────────┘
```

## Features

### Real-time Transcriptions
- Automatic broadcasting to all connected clients
- Includes confidence scores and timestamps
- Low-latency delivery

### Command Support
- `pause` - Stop listening
- `resume` - Start listening
- `get_status` - Query current state
- `set_language` - Change recognition language
- `set_model` - Switch Whisper model
- `set_vad_sensitivity` - Adjust voice detection

### Multi-client Support
- Unlimited concurrent connections
- Independent client management
- Thread-safe operation

### Robust Protocol
- JSON-based messages
- Length-prefixed framing
- Error handling and recovery

## Example Applications

### Voice Command System
```python
def handle_command(text):
    if "open browser" in text.lower():
        os.system("open -a Safari")
    elif "take screenshot" in text.lower():
        os.system("screencapture screenshot.png")

client.on_transcription(handle_command)
```

### Live Captioning
```python
def show_caption(text):
    # Display text overlay on screen
    display_subtitle(text)
    # Log for later
    with open("captions.log", "a") as f:
        f.write(f"{datetime.now()}: {text}\n")

client.on_transcription(show_caption)
```

### Translation Pipeline
```python
def translate_text(text):
    # Send to translation service
    translated = translate_api.translate(text, target='es')
    print(f"Spanish: {translated}")

client.on_transcription(translate_text)
```

## Testing

1. **Basic Connection Test**:
   ```bash
   # Terminal 1
   ./test_ipc.sh
   
   # Terminal 2
   ./test_python_client.sh
   ```

2. **Multiple Clients Test**:
   ```bash
   # Start multiple Python clients
   python3 examples/python_client.py &
   python3 examples/python_client.py &
   python3 examples/python_client.py &
   ```

3. **Command Test**:
   - Press 'p' to pause
   - Press 'r' to resume
   - Press 's' for status

## Protocol Details

See [IPC_PROTOCOL.md](IPC_PROTOCOL.md) for:
- Complete message format
- All command specifications
- Implementation examples
- Error handling details

## Next Steps

Now that the IPC server is working, we can:
1. Create the macOS daemon (launchd integration)
2. Build CLI tools for easy control
3. Develop client libraries for other languages
4. Implement the configuration system

The IPC server provides the foundation for all these features!
