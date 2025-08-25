# Real-Time STT Application Development Plan

## Executive Summary

This document outlines the complete development plan for a maximally efficient, low-latency, real-time Speech-to-Text (STT) application optimized for Apple Silicon Mac Mini M4. The application will run as a background daemon, providing continuous transcription services to other applications via IPC.

## Architecture Overview

### Core Components

1. **STT Engine**: whisper.cpp with streaming mode
2. **Audio Capture**: Core Audio with miniaudio fallback
3. **Voice Activity Detection**: Silero VAD or built-in energy-based VAD
4. **Background Service**: macOS launchd daemon
5. **IPC Mechanism**: Unix Domain Sockets
6. **Configuration System**: JSON-based with runtime updates

### Technology Stack

- **Language**: C++ (for maximum performance)
- **STT Model**: OpenAI Whisper (base.en for English, small/medium for multilingual)
- **Build System**: CMake
- **Dependencies**:
  - whisper.cpp (with Core ML acceleration)
  - miniaudio (audio capture)
  - nlohmann/json (configuration)
  - spdlog (logging)

## Detailed Component Design

### 1. Audio Pipeline

```
[MOTU M2] → [Core Audio] → [Ring Buffer] → [VAD] → [STT Engine] → [Text Output]
                                ↓
                          [Circular Buffer]
                                ↓
                          [Audio Monitor]
```

**Key Features:**
- Lock-free ring buffer for zero-copy audio handling
- Configurable buffer sizes (typical: 10-30ms chunks)
- Real-time priority thread for audio capture
- Energy-based VAD with adaptive thresholds

### 2. STT Engine Integration

**Whisper.cpp Configuration:**
- Use streaming mode with overlapping windows
- Implement sliding window approach (typical: 3s window, 0.5s step)
- Core ML acceleration for Apple Silicon
- Model variants:
  - `ggml-base.en.bin` (141MB) - English only, lowest latency
  - `ggml-small.bin` (466MB) - Multilingual, good balance
  - `ggml-medium.bin` (1.5GB) - Multilingual, higher accuracy

**Optimization Strategies:**
- Preload models at startup
- Use quantized models (Q4_0, Q5_1) for better performance
- Implement smart batching for efficiency
- Context caching for improved accuracy

### 3. IPC Server Design

**Unix Domain Socket Protocol:**
```
/tmp/rt-stt.sock (main socket)
/tmp/rt-stt-stream.sock (streaming socket)
```

**Message Protocol (JSON):**
```json
// Command Messages
{
  "type": "command",
  "action": "pause|resume|set_language|get_status",
  "params": {...}
}

// Stream Messages
{
  "type": "transcript",
  "text": "transcribed text",
  "timestamp": 1234567890,
  "confidence": 0.95,
  "is_final": true,
  "language": "en"
}
```

### 4. Background Daemon Architecture

**Process Structure:**
- Main daemon process (rt-stt-daemon)
- Audio capture thread (real-time priority)
- STT processing thread pool
- IPC server thread
- Configuration watcher thread

**Resource Management:**
- Dynamic CPU core allocation
- Memory pooling for audio buffers
- Graceful degradation under load

### 5. Configuration System

**Configuration File**: `/usr/local/etc/rt-stt/config.json`
```json
{
  "audio": {
    "device": "MOTU M2",
    "sample_rate": 16000,
    "channels": 1,
    "buffer_size_ms": 20
  },
  "stt": {
    "model": "base.en",
    "language": "en",
    "auto_detect": false,
    "beam_size": 5,
    "context_size": 3.0
  },
  "vad": {
    "enabled": true,
    "energy_threshold": 0.01,
    "silence_duration_ms": 500
  },
  "server": {
    "socket_path": "/tmp/rt-stt.sock",
    "max_clients": 10
  }
}
```

## Implementation Phases

### Phase 1: Core STT Engine (Week 1)
1. Set up whisper.cpp with Core ML
2. Implement basic transcription pipeline
3. Create simple CLI test tool
4. Benchmark performance

### Phase 2: Audio Capture & VAD (Week 1-2)
1. Implement Core Audio capture
2. Add ring buffer system
3. Implement VAD algorithm
4. Test with MOTU M2 device

### Phase 3: IPC Server (Week 2)
1. Create Unix socket server
2. Implement message protocol
3. Add streaming capabilities
4. Create client library

### Phase 4: Daemon Integration (Week 3)
1. Create launchd plist
2. Implement daemon lifecycle
3. Add logging and monitoring
4. Test auto-start on boot

### Phase 5: Configuration & Polish (Week 3-4)
1. Implement configuration system
2. Add runtime configuration updates
3. Create management CLI
4. Performance optimization

## Performance Targets

- **Latency**: < 200ms end-to-end (audio in → text out)
- **CPU Usage**: < 5% idle, < 20% active transcription
- **Memory**: < 500MB with base model, < 2GB with medium model
- **Accuracy**: > 95% WER for English (base.en model)

## Directory Structure

```
rt-stt/
├── CMakeLists.txt
├── README.md
├── DEVELOPMENT_PLAN.md
├── src/
│   ├── main.cpp
│   ├── daemon/
│   │   ├── daemon.cpp
│   │   └── daemon.h
│   ├── audio/
│   │   ├── capture.cpp
│   │   ├── capture.h
│   │   ├── vad.cpp
│   │   └── vad.h
│   ├── stt/
│   │   ├── engine.cpp
│   │   ├── engine.h
│   │   └── whisper_wrapper.cpp
│   ├── ipc/
│   │   ├── server.cpp
│   │   ├── server.h
│   │   └── protocol.h
│   └── config/
│       ├── config.cpp
│       └── config.h
├── client/
│   ├── librt-stt-client.cpp
│   ├── rt-stt-cli.cpp
│   └── examples/
├── scripts/
│   ├── install.sh
│   └── com.rt-stt.daemon.plist
├── tests/
│   ├── test_audio.cpp
│   ├── test_stt.cpp
│   └── test_ipc.cpp
└── models/
    └── download_models.sh
```

## Testing Strategy

1. **Unit Tests**: For individual components
2. **Integration Tests**: Audio → STT → IPC pipeline
3. **Performance Tests**: Latency and resource usage
4. **Stress Tests**: Multiple concurrent clients
5. **Real-world Tests**: Various accents and environments

## Security Considerations

- Unix socket permissions (0600)
- No network exposure
- Sandboxed daemon execution
- Secure model loading

## Next Steps

1. Review and approve this plan
2. Set up development environment
3. Begin Phase 1 implementation
4. Regular progress reviews

## Alternative Considerations

### Why These Choices?

**Whisper.cpp over alternatives:**
- Best accuracy for open-source models
- Excellent Apple Silicon optimization
- Active development and community
- Streaming support in recent versions

**Core Audio + miniaudio:**
- Core Audio for lowest latency on macOS
- miniaudio as fallback for portability
- Both are C/C++ for zero-overhead

**Unix Sockets over XPC:**
- Language agnostic (any client can connect)
- Lower overhead than XPC
- Simple protocol design
- Well-understood technology

**C++ over Rust/Go:**
- Zero-overhead abstractions
- Direct integration with whisper.cpp
- Best performance for real-time audio
- Mature ecosystem for audio processing

This plan prioritizes performance, reliability, and maintainability while meeting all specified requirements.
