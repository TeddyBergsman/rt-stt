# RT-STT Daemon Setup Guide

## Overview

The RT-STT daemon runs as a macOS LaunchAgent, providing always-on speech-to-text functionality that starts automatically when you log in.

## Installation

### Quick Install

```bash
./scripts/install_daemon.sh
```

This will:
1. Build the RT-STT binary
2. Install it to `/usr/local/bin/rt-stt`
3. Copy models to `/usr/local/share/rt-stt/models/`
4. Create configuration at `~/Library/Application Support/rt-stt/config.json`
5. Install and start the LaunchAgent

### Manual Installation

If you prefer to install manually:

1. **Build the project:**
   ```bash
   mkdir build && cd build
   cmake .. && make -j8
   ```

2. **Copy binary:**
   ```bash
   sudo cp build/rt-stt /usr/local/bin/
   ```

3. **Copy models:**
   ```bash
   sudo mkdir -p /usr/local/share/rt-stt/models
   sudo cp models/*.bin /usr/local/share/rt-stt/models/
   ```

4. **Create config directory:**
   ```bash
   mkdir -p ~/Library/Application\ Support/rt-stt
   cp config/default_config.json ~/Library/Application\ Support/rt-stt/config.json
   ```

5. **Install LaunchAgent:**
   ```bash
   cp com.rt-stt.user.plist ~/Library/LaunchAgents/
   sed -i '' "s/USER_NAME/$USER/g" ~/Library/LaunchAgents/com.rt-stt.user.plist
   launchctl load ~/Library/LaunchAgents/com.rt-stt.user.plist
   ```

## Configuration

Edit the configuration file at:
```
~/Library/Application Support/rt-stt/config.json
```

### Key Settings

**Audio Device:**
```json
"audio": {
  "device_name": "MOTU M2",
  "input_channel_index": 1  // 0 = Input 1, 1 = Input 2
}
```

**Model Selection:**
```json
"model": {
  "path": "/usr/local/share/rt-stt/models/ggml-small.en.bin"
}
```

**VAD Sensitivity:**
```json
"vad": {
  "speech_start_threshold": 1.08,  // Lower = more sensitive
  "speech_end_threshold": 0.85
}
```

## Control Commands

The daemon is controlled via `rt-stt-ctl`:

```bash
# Start the daemon
rt-stt-ctl start

# Stop the daemon
rt-stt-ctl stop

# Restart the daemon
rt-stt-ctl restart

# Check status
rt-stt-ctl status

# View live logs
rt-stt-ctl logs

# View error logs
rt-stt-ctl errors
```

## Using the Service

### Python Client

Connect to the running daemon:
```bash
rt-stt-client
```

Or in Python:
```python
from rt_stt import RTSTTClient

client = RTSTTClient()
client.connect()
client.on_transcription(lambda text: print(f">>> {text}"))
```

### Direct Socket Connection

The daemon listens on `/tmp/rt-stt.sock` by default.

## Logs

Logs are stored in:
- `~/Library/Logs/rt-stt.log` - Main log
- `~/Library/Logs/rt-stt.error.log` - Error log

## Troubleshooting

### Service Won't Start

1. Check error logs:
   ```bash
   rt-stt-ctl errors
   ```

2. Verify models exist:
   ```bash
   ls /usr/local/share/rt-stt/models/
   ```

3. Check permissions:
   ```bash
   ls -la /usr/local/bin/rt-stt
   ```

### No Audio Input

1. Check audio device in config
2. Verify microphone permissions in System Preferences
3. Test with different `input_channel_index`

### High CPU Usage

1. Adjust VAD sensitivity in config
2. Use smaller model (base.en instead of small.en)
3. Reduce `n_threads` in config

## Uninstallation

To completely remove the daemon:
```bash
./scripts/uninstall_daemon.sh
```

This will:
- Stop and unload the service
- Remove binaries
- Optionally remove models and configuration

## Security Notes

- The daemon runs as your user, not root
- It only activates when you're logged in
- Audio is processed locally, never sent to cloud
- IPC socket is user-accessible only

## Performance Tuning

### For Lower Latency
- Use `base.en` model
- Reduce `speech_end_ms` to 500-800
- Increase `process_priority` to -10

### For Better Accuracy
- Use `medium.en` model
- Increase `beam_size` to 10
- Enable `temperature` sampling (0.1-0.3)

### For Lower CPU
- Reduce `n_threads` to 2
- Increase `speech_start_threshold` to 1.2
- Use `base.en` model
