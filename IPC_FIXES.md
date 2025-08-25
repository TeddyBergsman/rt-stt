# IPC Fixes Summary

## Issues Found and Fixed

### 1. Message Type Mismatch ✅
**Problem**: Python client was looking for TRANSCRIPTION as type 4, but server sends it as type 3.

**Fix**: Updated Python client to use correct message type values:
- TRANSCRIPTION = 3 (was checking for 4)
- STATUS = 4 (was checking for 5)
- ERROR = 5 (was checking for 6)
- ACKNOWLEDGMENT = 6 (was checking for 7)

### 2. Message Framing Consistency ✅
**Problem**: Inconsistent handling of newlines in JSON messages.

**Fix**: 
- Server now sends raw JSON without newline
- Python client updated to match
- Length prefix correctly reflects actual message size

### 3. macOS Socket Signal Handling ✅
**Problem**: MSG_NOSIGNAL not available on macOS.

**Fix**:
- Added conditional compilation for macOS
- Set SO_NOSIGPIPE on client sockets
- Defined MSG_NOSIGNAL as 0 for macOS

### 4. Debug Output Added ✅
**Enhancement**: Added debug output to track message flow:
- Server shows when broadcasting transcriptions
- Server shows connected client count
- Server warns if transcriptions aren't sent
- Python client shows raw received messages (with debug flag)

## Testing the Fix

1. Start daemon: `./test_ipc_verbose.sh`
2. Connect client: `./test_python_client.sh`
3. Speak into microphone

You should now see:
- Daemon: `[DEBUG] Broadcasting to IPC clients...`
- Client: `>>> Your transcribed text`

## Protocol Reference

See [IPC_PROTOCOL.md](IPC_PROTOCOL.md) for complete protocol documentation.
