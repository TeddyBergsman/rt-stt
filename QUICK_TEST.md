# Quick IPC Test

## Terminal 1: Start daemon with debug output
```bash
./test_ipc_verbose.sh
```

Look for:
- "Broadcasting to IPC clients..."
- "Broadcast complete. Connected clients: X"

## Terminal 2: Test raw connection
```bash
./test_ipc_debug.py
```

This will show exactly what bytes are being sent/received.

## Terminal 3: Python client with debug
```bash
./test_python_client.sh
```

Look for "[DEBUG] Received message:" lines.

## What to check:

1. When you speak, Terminal 1 should show:
   - `[TRANSCRIPTION] your words`
   - `[DEBUG] Broadcasting to IPC clients...`
   - `[DEBUG] Broadcast complete. Connected clients: 1`

2. Terminal 2/3 should show the received messages

If Terminal 1 shows "Connected clients: 0", the client isn't properly registered.
