# RT-STT Configuration Management

## Overview

RT-STT provides comprehensive runtime configuration management through IPC, allowing you to modify almost any setting without restarting the daemon.

## Configuration Structure

The configuration is organized into several sections:

```json
{
  "model_config": {
    "model_path": "models/ggml-small.en.bin",
    "language": "en",
    "n_threads": 4,
    "use_gpu": true,
    "beam_size": 5,
    "temperature": 0.0
  },
  "vad_config": {
    "use_adaptive_threshold": true,
    "energy_threshold": 0.005,
    "speech_start_ms": 150,
    "speech_end_ms": 1000,
    "min_speech_ms": 500,
    "pre_speech_buffer_ms": 500,
    "noise_floor_adaptation_rate": 0.01,
    "speech_start_threshold": 1.08,
    "speech_end_threshold": 0.85
  },
  "audio_capture_config": {
    "device_name": "MOTU M2",
    "sample_rate": 16000,
    "channels": 1,
    "buffer_size_ms": 30,
    "force_single_channel": true,
    "input_channel_index": 1
  },
  "ipc_socket_path": "/tmp/rt-stt.sock",
  "enable_terminal_output": false,
  "measure_performance": true
}
```

## Configuration Commands

### Get Current Configuration

```bash
# Full configuration
rt-stt-cli get-config

# As JSON (for processing)
rt-stt-cli get-config -j

# Specific section with jq
rt-stt-cli get-config -j | jq '.vad_config'
```

### Update Configuration

#### 1. Quick Settings

```bash
# Change language
rt-stt-cli set-language es

# Adjust VAD sensitivity
rt-stt-cli set-vad-sensitivity 1.2

# Switch model
rt-stt-cli set-model models/ggml-medium.en.bin
```

#### 2. VAD Configuration

```bash
# Update multiple VAD settings
rt-stt-cli set-vad \
    --start-threshold 1.1 \
    --end-threshold 0.9 \
    --speech-start-ms 200 \
    --speech-end-ms 800 \
    --min-speech-ms 400 \
    --energy-threshold 0.008
```

#### 3. Full Configuration Update

```bash
# Create config file
cat > custom-config.json << EOF
{
  "vad_config": {
    "speech_start_threshold": 1.05,
    "speech_end_threshold": 0.88
  },
  "model_config": {
    "language": "es",
    "temperature": 0.1
  }
}
EOF

# Apply configuration
rt-stt-cli set-config "$(cat custom-config.json)"

# Apply without saving to file
rt-stt-cli set-config "$(cat custom-config.json)" --no-save
```

### Performance Monitoring

```bash
# Get performance metrics
rt-stt-cli get-metrics

# As JSON for monitoring scripts
rt-stt-cli get-metrics -j
```

## Python API

### Basic Usage

```python
from rt_stt import RTSTTClient

client = RTSTTClient()
client.connect()

# Get configuration
config = client.get_config()
print(f"Current language: {config['model_config']['language']}")

# Update VAD settings
client.set_vad_config(
    speech_start_threshold=1.1,
    speech_end_threshold=0.9,
    min_speech_ms=400
)

# Update model settings
client.set_model_config(
    language='es',
    temperature=0.1
)

# Full config update
new_config = {
    "vad_config": {
        "speech_start_threshold": 1.05
    }
}
client.set_config(new_config)

# Get metrics
metrics = client.get_metrics()
print(f"Average latency: {metrics['avg_latency_ms']}ms")
```

### Advanced Configuration

```python
# Dynamic VAD adjustment based on environment
def adjust_vad_for_environment(client, noise_level):
    if noise_level > 0.8:
        # Noisy environment
        client.set_vad_config(
            speech_start_threshold=1.2,
            speech_end_threshold=0.9,
            min_speech_ms=600
        )
    else:
        # Quiet environment
        client.set_vad_config(
            speech_start_threshold=1.05,
            speech_end_threshold=0.85,
            min_speech_ms=400
        )

# Performance-based model switching
def optimize_for_latency(client):
    metrics = client.get_metrics()
    if metrics['avg_latency_ms'] > 500:
        # Switch to smaller model
        client.set_model('models/ggml-base.en.bin')
```

## Configuration Parameters

### Model Configuration

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| model_path | string | Path to Whisper model | models/ggml-small.en.bin |
| language | string | Recognition language | en |
| n_threads | int | CPU threads to use | 4 |
| use_gpu | bool | Enable GPU acceleration | true |
| beam_size | int | Beam search width | 5 |
| temperature | float | Sampling temperature | 0.0 |

### VAD Configuration

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| use_adaptive_threshold | bool | Enable adaptive thresholding | true |
| energy_threshold | float | Base energy threshold | 0.005 |
| speech_start_ms | int | Time to confirm speech start | 150 |
| speech_end_ms | int | Time to confirm speech end | 1000 |
| min_speech_ms | int | Minimum speech duration | 500 |
| pre_speech_buffer_ms | int | Pre-speech buffer size | 500 |
| noise_floor_adaptation_rate | float | Noise adaptation rate | 0.01 |
| speech_start_threshold | float | Start threshold multiplier | 1.08 |
| speech_end_threshold | float | End threshold multiplier | 0.85 |

### Audio Configuration

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| device_name | string | Audio device name | MOTU M2 |
| sample_rate | int | Sample rate (Hz) | 16000 |
| channels | int | Number of channels | 1 |
| buffer_size_ms | int | Buffer size (ms) | 30 |
| force_single_channel | bool | Force single channel | true |
| input_channel_index | int | Channel to use (0-based) | 1 |

## Best Practices

### 1. VAD Tuning

For optimal results, tune VAD based on your environment:

**Quiet Office/Studio:**
```bash
rt-stt-cli set-vad \
    --start-threshold 1.05 \
    --end-threshold 0.85 \
    --min-speech-ms 400
```

**Noisy Environment:**
```bash
rt-stt-cli set-vad \
    --start-threshold 1.2 \
    --end-threshold 0.9 \
    --min-speech-ms 600 \
    --energy-threshold 0.01
```

**Fast Response (Commands):**
```bash
rt-stt-cli set-vad \
    --speech-start-ms 100 \
    --speech-end-ms 500 \
    --min-speech-ms 300
```

### 2. Model Selection

- **ggml-base.en.bin**: Fastest, lower accuracy (~100ms latency)
- **ggml-small.en.bin**: Balanced (~200ms latency)
- **ggml-medium.en.bin**: Best accuracy (~400ms latency)

### 3. Configuration Persistence

- Changes are saved by default
- Use `--no-save` for temporary changes
- Config file location: `~/Library/Application Support/rt-stt/config.json`

### 4. Monitoring and Adjustment

```bash
# Monitor performance
watch -n 1 'rt-stt-cli get-metrics -j | jq .'

# Auto-adjust based on metrics
while true; do
    LATENCY=$(rt-stt-cli get-metrics -j | jq '.avg_latency_ms')
    if (( $(echo "$LATENCY > 300" | bc -l) )); then
        rt-stt-cli set-model models/ggml-base.en.bin
    fi
    sleep 60
done
```

## Troubleshooting

### Configuration Not Taking Effect

1. Check if daemon is running: `rt-stt-cli status`
2. Verify config was applied: `rt-stt-cli get-config`
3. Check logs: `rt-stt-ctl logs`

### Invalid Configuration

The daemon validates configuration before applying. Invalid settings will return an error:

```bash
rt-stt-cli set-vad --speech-start-ms -100
# Error: Invalid speech_start_ms: must be positive
```

### Performance Impact

Some changes may impact performance:
- Model changes require reloading (brief pause)
- VAD changes apply immediately
- Audio settings may require device reinitialization
