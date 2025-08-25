# RT-STT CLI Tools Documentation

## Overview

RT-STT provides both C++ and Python command-line interfaces for interacting with the daemon. Both offer similar functionality with slightly different features.

## Installation

After building and installing RT-STT:
```bash
./scripts/install_daemon.sh
```

You'll have access to:
- `rt-stt-cli` - C++ CLI tool (fast, minimal dependencies)
- `rt-stt-cli` - Python CLI tool (when Python package is installed)

## C++ CLI Tool

### Basic Usage

```bash
# Stream transcriptions (default)
rt-stt-cli

# Stream with timestamps
rt-stt-cli stream -t

# Stream as JSON
rt-stt-cli stream -j

# Get status
rt-stt-cli status

# Control commands
rt-stt-cli pause
rt-stt-cli resume

# Set language
rt-stt-cli set-language es
```

### Options

- `-s, --socket PATH` - Specify socket path (default: /tmp/rt-stt.sock)
- `-j, --json` - Output in JSON format
- `-t, --timestamp` - Include timestamps
- `-h, --help` - Show help

### Examples

```bash
# Stream to a file
rt-stt-cli stream > transcription.txt

# Stream JSON for processing
rt-stt-cli stream -j | jq '.text'

# Use with custom socket
rt-stt-cli status -s /tmp/custom-stt.sock
```

## Python CLI Tool

The Python CLI offers additional features like colored output and monitoring.

### Installation

```bash
pip3 install --user ./python
```

### Basic Usage

```bash
# Stream transcriptions
rt-stt-cli stream

# Stream with options
rt-stt-cli stream -t -c  # timestamps + confidence

# Monitor with live stats
rt-stt-cli monitor

# Get detailed status
rt-stt-cli status
```

### Advanced Features

1. **Colored Output** - Status indicators and errors in color
2. **Auto-reconnect** - Automatically reconnects if daemon restarts
3. **Live Monitoring** - Real-time statistics
4. **Quiet Mode** - Suppress status messages for piping

### Python Library Usage

```python
from rt_stt import RTSTTClient

# Simple usage
with RTSTTClient() as client:
    def on_transcription(result):
        print(f"Heard: {result.text}")
    
    client.on_transcription(on_transcription)
    client.start_listening()
    
    # Keep running
    input("Press Enter to stop...")

# Advanced usage
client = RTSTTClient(auto_reconnect=True)
client.connect()

# Get status
status = client.get_status()
print(f"Model: {status.model}")
print(f"Language: {status.language}")

# Control daemon
client.pause()
client.set_language('es')
client.resume()
```

## Common Use Cases

### 1. Live Transcription to File

```bash
# With timestamps
rt-stt-cli stream -t > meeting_notes.txt

# As JSON for later processing
rt-stt-cli stream -j > transcript.jsonl
```

### 2. Voice Commands

```bash
rt-stt-cli stream | while read -r line; do
    case "$line" in
        *"open browser"*) open -a Safari ;;
        *"take screenshot"*) screencapture screenshot.png ;;
        *"what time"*) say "It's $(date +%l:%M)" ;;
    esac
done
```

### 3. Real-time Translation

```bash
# Pipe to translation service
rt-stt-cli stream | translate-cli --from en --to es
```

### 4. Monitoring

```python
# Custom monitoring script
import rt_stt

client = rt_stt.RTSTTClient()
client.connect()

transcription_count = 0

def on_transcription(result):
    global transcription_count
    transcription_count += 1
    if transcription_count % 10 == 0:
        print(f"Processed {transcription_count} transcriptions")

client.on_transcription(on_transcription)
client.start_listening()
```

## Output Formats

### Plain Text (Default)
```
Hello world
This is a test
```

### With Timestamps (-t)
```
[14:23:45] Hello world
[14:23:47] This is a test
```

### JSON Format (-j)
```json
{"text": "Hello world", "confidence": 0.95, "timestamp": 1699123456789}
{"text": "This is a test", "confidence": 0.92, "timestamp": 1699123458123}
```

## Tips

1. **Performance**: C++ CLI has lower overhead for simple streaming
2. **Features**: Python CLI better for complex processing and monitoring
3. **Piping**: Use `-q` flag in Python CLI to suppress status messages
4. **Scripting**: JSON output makes parsing easier
5. **Debugging**: Use `rt-stt-ctl logs` to debug connection issues

## Troubleshooting

### "Failed to connect" Error
- Check if daemon is running: `rt-stt-ctl status`
- Verify socket path exists: `ls -la /tmp/rt-stt.sock`
- Check logs: `rt-stt-ctl logs`

### No Output
- Ensure daemon is listening: `rt-stt-cli status`
- Check if paused: `rt-stt-cli resume`
- Verify audio input in logs

### High CPU Usage
- Consider using C++ CLI for simple streaming
- Adjust VAD sensitivity: `rt-stt-cli set-vad-sensitivity 1.2`
