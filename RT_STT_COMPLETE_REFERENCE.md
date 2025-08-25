# RT-STT Complete Reference

This document provides a complete reference for all RT-STT functionality.

## Table of Contents
1. [Language Settings](#language-settings)
2. [Model Management](#model-management)
3. [VAD Configuration](#vad-configuration)
4. [Audio Configuration](#audio-configuration)
5. [IPC Commands](#ipc-commands)
6. [Python Client Methods](#python-client-methods)
7. [CLI Commands](#cli-commands)
8. [Configuration File Format](#configuration-file-format)

## Language Settings

### Setting Language to Auto-detect
```bash
# CLI
rt-stt-cli set-language auto

# Python
client.set_language("auto")

# Raw IPC
{"command": "set_language", "data": {"language": "auto"}}
```

### Available Languages for Multilingual Models
- `"auto"` - Automatic detection (99 languages)
- `"en"` - English
- `"zh"` - Chinese (中文)
- `"de"` - German (Deutsch)
- `"es"` - Spanish (Español)
- `"ru"` - Russian (Русский)
- `"ko"` - Korean (한국어)
- `"fr"` - French (Français)
- `"ja"` - Japanese (日本語)
- `"pt"` - Portuguese (Português)
- `"tr"` - Turkish (Türkçe)
- `"pl"` - Polish (Polski)
- `"ca"` - Catalan (Català)
- `"nl"` - Dutch (Nederlands)
- `"ar"` - Arabic (العربية)
- `"sv"` - Swedish (Svenska)
- `"it"` - Italian (Italiano)
- `"id"` - Indonesian (Indonesia)
- `"hi"` - Hindi (हिन्दी)
- `"fi"` - Finnish (Suomi)
- `"vi"` - Vietnamese (Tiếng Việt)
- `"he"` - Hebrew (עברית)
- `"uk"` - Ukrainian (Українська)
- `"el"` - Greek (Ελληνικά)
- `"ms"` - Malay (Bahasa Melayu)
- `"cs"` - Czech (Čeština)
- `"ro"` - Romanian (Română)
- `"da"` - Danish (Dansk)
- `"hu"` - Hungarian (Magyar)
- `"ta"` - Tamil (தமிழ்)
- `"no"` - Norwegian (Norsk)
- `"th"` - Thai (ไทย)
- And 60+ more languages...

## Model Management

### Switching Models
```bash
# CLI
rt-stt-cli set-model models/ggml-large-v3.bin

# Python
client.set_model("models/ggml-large-v3.bin")

# For multilingual with auto-detection
client.set_model("models/ggml-large-v3.bin")
client.set_language("auto")
```

### Model Recommendations
- **English only, fastest**: `ggml-small.en.bin`
- **English only, best**: `ggml-medium.en.bin`
- **Multilingual, balanced**: `ggml-small.bin` + `language="auto"`
- **Multilingual, best**: `ggml-large-v3.bin` + `language="auto"`

## VAD Configuration

### Simple Sensitivity Adjustment
```bash
# CLI
rt-stt-cli set-vad-sensitivity 1.08

# Python
client.set_vad_sensitivity(1.08)
```

### Advanced VAD Settings
```python
# Python
client.set_vad_config(
    speech_start_threshold=1.08,    # Multiplier for speech detection
    speech_end_threshold=0.85,      # Multiplier for silence detection
    min_speech_ms=100,              # Minimum speech duration
    max_speech_ms=30000,            # Maximum speech duration
    speech_timeout_ms=300,          # Time to wait before ending
    silence_timeout_ms=1000,        # Silence before stopping
    pre_speech_buffer_ms=500        # Audio to keep before speech
)

# CLI
rt-stt-cli set-vad --start-threshold 1.08 --end-threshold 0.85
```

## Audio Configuration

### Changing Input Channel
```json
// Configuration file only (requires restart)
{
    "capture_config": {
        "input_channel_index": 0    // 0=Input1, 1=Input2
    }
}
```

## IPC Commands

### Complete Command List
1. `subscribe` - Start receiving transcriptions
2. `unsubscribe` - Stop receiving transcriptions
3. `pause` - Pause listening (keeps connection)
4. `resume` - Resume listening
5. `get_status` - Get current status
6. `get_config` - Get full configuration
7. `set_config` - Update configuration
8. `set_language` - Change language
9. `set_model` - Change model
10. `set_vad_sensitivity` - Adjust VAD
11. `get_metrics` - Get performance stats

## Python Client Methods

### Connection Management
```python
client = RTSTTClient(socket_path="/tmp/rt-stt.sock")
client.connect()
client.disconnect()
client.is_connected()
```

### Transcription Methods
```python
# Blocking generator (recommended)
for transcription in client.transcriptions():
    print(transcription.text)

# Callback-based (non-blocking)
client.on_transcription(callback)
client.start_listening()
client.stop_listening()
```

### Control Methods
```python
client.subscribe()
client.unsubscribe()
client.pause()
client.resume()
```

### Configuration Methods
```python
# Simple
client.set_language("auto")
client.set_model("models/ggml-large-v3.bin")
client.set_vad_sensitivity(1.08)

# Advanced
client.set_vad_config(**params)
client.set_model_config(**params)
client.set_config(config_dict, save=True)

# Query
status = client.get_status()
config = client.get_config()
metrics = client.get_metrics()
```

### Event Callbacks
```python
client.on_transcription(lambda r: print(r.text))
client.on_error(lambda e: print(f"Error: {e}"))
client.on_connection(lambda c: print(f"Connected: {c}"))
client.on_status(lambda s: print(f"Status: {s}"))
```

## CLI Commands

### Basic Usage
```bash
# Stream transcriptions
rt-stt-cli stream                  # Simple text
rt-stt-cli stream -j               # JSON format
rt-stt-cli stream -t               # With timestamps
rt-stt-cli stream -c               # With confidence

# Control
rt-stt-cli pause
rt-stt-cli resume
rt-stt-cli status

# Configuration
rt-stt-cli set-language auto
rt-stt-cli set-model models/ggml-large-v3.bin
rt-stt-cli get-config
rt-stt-cli get-metrics

# System control
rt-stt-ctl start
rt-stt-ctl stop
rt-stt-ctl restart
rt-stt-ctl status
rt-stt-ctl logs
```

## Configuration File Format

### Complete Configuration Example
```json
{
    "model_config": {
        "model_path": "/usr/local/share/rt-stt/models/ggml-small.en.bin",
        "language": "auto",              // or specific language code
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
        "input_channel_index": 1         // 0=Input1, 1=Input2
    },
    "ipc_socket_path": "/tmp/rt-stt.sock"
}
```

### Configuration File Location
- User config: `~/Library/Application Support/rt-stt/config.json`
- System default: `/usr/local/share/rt-stt/config/default_config.json`

## Common Use Cases

### 1. English-only, Maximum Speed
```python
client.set_model("models/ggml-small.en.bin")
client.set_language("en")
```

### 2. Auto Language Detection
```python
client.set_model("models/ggml-large-v3.bin")
client.set_language("auto")
```

### 3. Specific Language (Spanish)
```python
client.set_model("models/ggml-medium.bin")
client.set_language("es")
```

### 4. Quiet Environment Settings
```python
client.set_vad_config(
    speech_start_threshold=1.2,  # Less sensitive
    speech_end_threshold=0.9
)
```

### 5. Noisy Environment Settings
```python
client.set_vad_config(
    speech_start_threshold=1.05,  # More sensitive
    speech_end_threshold=0.8,
    min_speech_ms=200  # Ignore brief sounds
)
```

## Troubleshooting

### No Auto Language Detection
- Ensure you're using a multilingual model (without `.en`)
- Set language to `"auto"` explicitly
- Check model supports target language

### Wrong Language Detected
- Use larger model (medium or large-v3)
- Speak more for better detection
- Set specific language if known

### Connection Issues
```bash
# Check daemon is running
rt-stt-ctl status

# Check socket exists
ls -la /tmp/rt-stt.sock

# View logs
rt-stt-ctl logs
```
