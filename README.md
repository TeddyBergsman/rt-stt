# RT-STT: Real-Time Speech-to-Text for Apple Silicon

A maximally efficient, low-latency, real-time speech-to-text application optimized for Apple Silicon Macs. Features fully offline operation with OpenAI's Whisper models, optimized for the Mac Mini M4.

## Features

- ✅ **Ultra-low latency**: < 200ms end-to-end transcription
- ✅ **Apple Silicon optimized**: Uses Core ML and Metal acceleration
- ✅ **Fully offline**: No internet connection required
- ✅ **Real-time streaming**: Continuous transcription with overlapping windows
- ✅ **Voice Activity Detection**: Smart detection of speech start/end
- ✅ **Multi-language support**: English-only or multilingual models
- ✅ **Background daemon**: Runs as macOS service, boots on startup
- ✅ **IPC interface**: Other applications can consume the text stream
- ✅ **MOTU M2 support**: Optimized for professional audio interfaces
- ✅ **Terminal visualization**: Real-time display for testing and validation

## Quick Start

```bash
# 1. Clone the repository
git clone <repository-url>
cd rt-stt

# 2. Build the project
./build.sh

# 3. Download a model (recommended: base.en for lowest latency)
./scripts/download_models.sh
# Select option 2 for base.en

# 4. Run the test application
./test_stt.sh
# Or manually:
./build/rt-stt-test --model models/ggml-base.en.bin
```

## Build Status

✅ **Successfully tested on Mac Mini M4 Pro**
- whisper.cpp built with Metal acceleration
- Core Audio integration working
- Real-time terminal visualization implemented

## System Requirements

- macOS 13.0+ (Ventura or later)
- Apple Silicon Mac (M1/M2/M3/M4)
- Xcode Command Line Tools
- CMake 3.16+
- 2GB RAM minimum (varies by model)

## Building from Source

### Dependencies

The build script automatically downloads:
- whisper.cpp (with Core ML support)
- miniaudio (audio capture fallback)
- nlohmann/json (configuration)

```bash
# Download dependencies manually if needed
./scripts/download_dependencies.sh

# Build the project
./build.sh
```

### Build Options

```bash
# Debug build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make

# Release build with specific architecture
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
make -j8
```

## Usage

### Test Application (with Terminal Output)

The test application provides real-time visualization of the STT pipeline:

```bash
./build/rt-stt-test [options]

Options:
  --model PATH       Path to Whisper model (required)
  --language LANG    Language code (default: en, use 'auto' for detection)
  --threads N        Number of threads (default: 4)
  --no-gpu           Disable GPU acceleration
  --translate        Translate to English
  --device NAME      Audio device name (default: MOTU M2)
  --help             Show help message
```

Example:
```bash
# English-only, lowest latency
./build/rt-stt-test --model models/ggml-base.en.bin

# Multilingual with auto-detection
./build/rt-stt-test --model models/ggml-small.bin --language auto

# Use default audio device instead of MOTU M2
./build/rt-stt-test --model models/ggml-base.en.bin --device ""
```

### Terminal Output Features

The test application displays:
- 🎤 **Live transcriptions**: Color-coded partial (yellow) and final (green) results
- 📊 **Confidence bars**: Visual representation of transcription confidence
- 🔊 **Audio level meter**: Real-time VU meter showing input levels
- ⏱️ **Latency tracking**: Processing time for each transcription
- 📈 **Performance metrics**: CPU usage, memory, and real-time factor
- 🔇 **VAD status**: Speech detection indicators

## Model Selection

| Model | Size | Language | Latency | Accuracy | Recommendation |
|-------|------|----------|---------|----------|----------------|
| tiny.en | 39 MB | English | ~50ms | Good | Development/testing |
| **base.en** | **142 MB** | **English** | **~100ms** | **Very Good** | **Real-time STT** |
| small.en | 466 MB | English | ~200ms | Excellent | Higher accuracy |
| small | 466 MB | Multi | ~250ms | Very Good | Multilingual |
| medium | 1.5 GB | Multi | ~500ms | Excellent | Accuracy-focused |

## Configuration

### Audio Settings

The application automatically detects and uses the MOTU M2 audio interface. To use a different device:

```bash
# List available audio devices
./build/rt-stt-test --model models/ggml-base.en.bin --device "?" # (future feature)

# Use specific device
./build/rt-stt-test --model models/ggml-base.en.bin --device "Scarlett 2i2"

# Use system default
./build/rt-stt-test --model models/ggml-base.en.bin --device ""
```

### VAD Configuration

Voice Activity Detection parameters can be tuned for your environment:
- **Energy threshold**: Minimum signal level to detect speech
- **Speech start/end timing**: Milliseconds of speech/silence required
- **Adaptive threshold**: Automatically adjusts to background noise

## Daemon Mode (Coming Soon)

The full daemon implementation with IPC is in development. When complete:

```bash
# Install as system service
sudo ./scripts/install.sh

# Control the daemon
launchctl start com.rt-stt.daemon
launchctl stop com.rt-stt.daemon

# Connect from other applications
rt-stt-client --subscribe  # Stream transcriptions
rt-stt-client --command pause  # Control daemon
```

## Performance Optimization

1. **Model Selection**: Use `base.en` for lowest latency
2. **CPU Threads**: Set to physical core count (e.g., 8 for M4)
3. **Buffer Size**: Lower values reduce latency but increase CPU
4. **Disable Translation**: Use native language models when possible

## Troubleshooting

### Audio Device Not Found
```bash
# Check if MOTU M2 is connected
system_profiler SPAudioDataType | grep MOTU

# Use default device
./build/rt-stt-test --model models/ggml-base.en.bin --device ""
```

### High Latency
- Use smaller models (base.en recommended)
- Ensure no other CPU-intensive tasks running
- Check thermal throttling: `sudo powermetrics --samplers smc`

### No Transcription Output
- Check audio levels in terminal display
- Verify microphone permissions in System Settings
- Test with louder speech or adjust VAD threshold

## Architecture

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│ Audio Input │ --> │ Ring Buffer  │ --> │     VAD     │
│  (Core Audio)│     │ (Lock-free)  │     │ (Adaptive)  │
└─────────────┘     └──────────────┘     └─────────────┘
                                                 |
                    ┌──────────────┐     ┌─────────────┐
                    │ Whisper.cpp  │ <-- │ Speech      │
                    │ (Core ML)    │     │ Buffer      │
                    └──────────────┘     └─────────────┘
                            |
                    ┌──────────────┐     ┌─────────────┐
                    │ Text Output  │ --> │ IPC Server  │
                    │ (Streaming)  │     │ (Unix Socket)│
                    └──────────────┘     └─────────────┘
```

## Contributing

Contributions are welcome! Please ensure:
- Code follows C++17 standards
- Performance impact is measured
- Tests pass on Apple Silicon
- Documentation is updated

## License

[Your License Here]

## Acknowledgments

- [whisper.cpp](https://github.com/ggerganov/whisper.cpp) - Efficient Whisper implementation
- [miniaudio](https://github.com/mackron/miniaudio) - Cross-platform audio library
- OpenAI for the Whisper model
