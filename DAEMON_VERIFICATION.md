# RT-STT Daemon Verification ✅

## Installation Status

### 1. Daemon Binary
- **Location**: `/usr/local/bin/rt-stt`
- **Updated**: Yes (latest build with full metadata)
- **Size**: ~1MB

### 2. Running Status
- **Service**: `com.rt-stt.user`
- **Status**: Running
- **Auto-start**: Yes (via launchctl)
- **Socket**: `/tmp/rt-stt.sock`

### 3. Configuration
- **Config File**: `~/Library/Application Support/rt-stt/config.json`
- **Models**: `/usr/local/share/rt-stt/models/`
- **Logs**: `~/Library/Logs/rt-stt.log`

## How to Test

### Basic Test (Simple Output)
```bash
# Stream transcriptions
rt-stt-cli stream

# Check status
rt-stt-cli status
```

### Full Metadata Test (JSON Output)
```bash
# Stream with complete metadata
rt-stt-cli stream -j

# Pretty print with jq
rt-stt-cli stream -j | jq '.'
```

### Expected JSON Output
You should now see ALL metadata including:
- `text`: Transcribed text
- `confidence`: Real confidence score
- `language`: Detected language
- `language_probability`: Language confidence
- `processing_time_ms`: Actual processing time
- `audio_duration_ms`: Audio length
- `model`: Model filename
- `segments`: Array with token IDs, timing, etc.

## Control Commands

### System Control
```bash
rt-stt-ctl start    # Start daemon
rt-stt-ctl stop     # Stop daemon
rt-stt-ctl restart  # Restart daemon
rt-stt-ctl status   # Check status
rt-stt-ctl logs     # View logs
```

### Configuration
```bash
# Change language
python3 -m rt_stt.cli set-language es

# Change model
python3 -m rt_stt.cli set-model models/ggml-medium.en.bin

# Adjust VAD
python3 -m rt_stt.cli set-vad --start-threshold 1.1
```

## Verification Checklist

✅ **Daemon installed**: `/usr/local/bin/rt-stt` exists
✅ **Service running**: `launchctl list | grep rt-stt` shows PID
✅ **Socket active**: `/tmp/rt-stt.sock` exists
✅ **CLI working**: `rt-stt-cli status` responds
✅ **Auto-start enabled**: Will start on system boot
✅ **Full metadata**: JSON output includes all fields

## Quick Health Check

Run this command:
```bash
rt-stt-cli status && echo "✅ Daemon is healthy!"
```

If you see the status and "Daemon is healthy!", everything is working perfectly!

## Troubleshooting

If something isn't working:

1. **Check logs**:
   ```bash
   rt-stt-ctl logs
   ```

2. **Restart daemon**:
   ```bash
   rt-stt-ctl restart
   ```

3. **Verify socket**:
   ```bash
   ls -la /tmp/rt-stt.sock
   ```

4. **Check process**:
   ```bash
   ps aux | grep rt-stt
   ```

Your RT-STT daemon is now fully installed and will automatically start when you log in!
