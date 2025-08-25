# Troubleshooting RT-STT IPC Connection

## Common Issues and Solutions

### 1. No Transcriptions Appearing

**Symptoms:**
- Python client connects successfully
- Commands work (pause/resume)
- No transcriptions appear

**Solutions:**

1. **Check Audio Capture**
   - Ensure your microphone is connected to Input 2 of MOTU M2
   - The daemon output should show: `Device has 2 input channels` and `Will use channel 2 (Input 2)`
   - If using a different input, edit `src/audio/capture.h` and change `input_channel_index`

2. **Check Model Loading**
   - Daemon output should show: `Whisper model loaded successfully`
   - If not, ensure model file exists: `models/ggml-small.en.bin`

3. **Check VAD Detection**
   - Daemon should show VAD state changes when you speak
   - If not, VAD might be too insensitive
   - Try speaking louder or closer to the mic

4. **Restart Both Processes**
   ```bash
   # Terminal 1 - Stop with Ctrl+C and restart
   ./test_ipc_verbose.sh
   
   # Terminal 2 - Stop with 'q' and restart
   ./test_python_client.sh
   ```

### 2. "Error: Unknown error" Messages

This is usually a harmless message from the Python client receiving multiple responses. You can ignore it if commands are working.

### 3. Connection Refused

**Error:** `Failed to connect: [Errno 61] Connection refused`

**Solutions:**
1. Ensure daemon is running first
2. Check socket path matches: `/tmp/rt-stt-test.sock`
3. Remove stale socket: `rm -f /tmp/rt-stt-test.sock`

### 4. Audio Device Not Found

**Error:** `Audio device not found: MOTU M2`

**Solutions:**
1. Check device is connected and powered on
2. The daemon will fall back to default device
3. To use default mic, edit main.cpp: `capture_config.device_name = "";`

## Testing Audio Flow

### 1. Test with Simple STT First
```bash
# Verify basic STT works
./test_sensitive.sh
```

### 2. Check Daemon Audio
When running `./test_ipc_verbose.sh`, you should see:
```
Initializing audio capture...
Looking for device: MOTU M2
Device has 2 input channels
Will use channel 2 (Input 2)
```

### 3. Monitor IPC Messages
The Python client shows all messages. During normal operation:
- STATUS messages every 30 seconds
- TRANSCRIPTION messages when you speak
- ACKNOWLEDGMENT for commands

### 4. Debug Commands
In the Python client:
- `s` - Get status (shows if listening)
- `p` - Pause (should stop transcriptions)
- `r` - Resume (should restart transcriptions)

## Advanced Debugging

### Enable More Logging

Edit `src/main.cpp` and add after STT engine initialization:
```cpp
stt_config.enable_terminal_output = true;
```

This will show detailed VAD and processing info in the daemon terminal.

### Test with netcat

You can test raw socket connection:
```bash
# See if socket exists
ls -la /tmp/rt-stt-test.sock

# Try connecting with netcat
nc -U /tmp/rt-stt-test.sock
```

### Check Process Status
```bash
# See if daemon is running
ps aux | grep rt-stt

# Check CPU usage (should be ~5-10% when active)
top | grep rt-stt
```

## Still Having Issues?

1. Make sure you've downloaded the models
2. Try the simpler test first: `./test_sensitive.sh`
3. Check that audio input is working in System Preferences
4. Ensure no other process is using the microphone
