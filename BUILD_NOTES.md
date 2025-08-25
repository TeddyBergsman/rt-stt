# Build Notes

## Issues Fixed

1. **whisper.cpp Build Issues**
   - Fixed CMakeLists.txt to use pre-built whisper.cpp libraries from build directory
   - Updated to use CMake build system instead of Makefile for whisper.cpp
   - Linked against dynamic libraries (.dylib) instead of static

2. **Include Path Issues**
   - Added proper include directories for whisper.cpp headers
   - Fixed namespace issues with VADConfig (added audio:: namespace)

3. **API Compatibility**
   - Updated deprecated whisper_init_from_file to whisper_init_from_file_with_params
   - Changed WHISPER_DECODE_* enums to WHISPER_SAMPLING_*
   - Removed non-existent struct fields (speed_up, suppress_non_speech_tokens)
   - Fixed WHISPER_SAMPLE_RATE macro conflict

4. **Core Audio Integration**
   - Fixed friend function declaration for CoreAudioCallback
   - Made process_audio_callback public for C callback access
   - Updated deprecated kAudioObjectPropertyElementMaster to kAudioObjectPropertyElementMain

5. **Compilation Fixes**
   - Fixed integer overflow in memory size calculation
   - Removed override keyword from non-virtual function
   - Added missing config.cpp to test build

## Current Status

✅ **Working Features:**
- whisper.cpp integration with Metal acceleration
- Core Audio capture (with miniaudio fallback)
- Voice Activity Detection (VAD) - Fixed threshold calculation
- Real-time terminal visualization
- Model loading and initialization
- Command-line argument parsing
- Basic speech transcription (needs tuning)

🔧 **Recent Fixes (Nov 2024):**
- Fixed VAD threshold calculation (was dividing instead of multiplying)
- **MAJOR FIX**: Removed sliding window processing that caused repetition
- **MAJOR FIX**: Now processes complete utterances only (waits for sentence end)
- **MAJOR FIX**: Increased VAD speech_end_ms to 800ms (detects sentence boundaries)
- **MAJOR FIX**: Added minimum utterance length (0.5s) to avoid fragments
- **CRITICAL FIX**: Fixed VAD state transition detection (was looking for SPEECH->SILENCE, but actual transition is SPEECH_ENDING->SILENCE)
- **CRITICAL FIX**: Implemented pre-speech buffer to capture first words
- Reduced debug output for cleaner display
- Improved text filtering to skip punctuation-only results
- Adjusted VAD sensitivity (1.05x noise floor for normal speaking voice)
- Added faster noise floor adaptation (0.01 rate)
- Clear speech buffer on new utterance start

⚠️ **Known Issues:**
- Base model has poor accuracy (use small.en or medium.en)
- Need to implement proper streaming for real-time feedback
- Could add partial results during long utterances

⚠️ **Not Yet Implemented:**
- IPC server (Unix domain sockets)
- macOS daemon integration
- Full configuration system
- Client libraries

## Testing

The application successfully:
1. Loads Whisper models
2. Initializes Metal GPU acceleration
3. Detects Apple M4 Pro hardware
4. Allocates audio processing resources
5. Captures audio (meter shows activity)

## Debugging STT Issues

If audio is captured but no transcriptions appear:

### 1. Run the debug script:
```bash
./debug_stt.sh --no-vad  # Forces processing of ALL audio
# or
./debug_stt.sh           # Normal VAD mode
```

### 2. Check the debug output for:

**Audio Input Stats** (every 100 callbacks):
- `Audio stats: max=X, avg=Y` - Shows if audio samples are non-zero
- Should see max > 0.01 when speaking

**VAD State Transitions**:
- `VAD State: SILENCE → SPEECH_MAYBE → SPEECH → SPEECH_ENDING`
- Shows energy levels and noise floor

**Speech Buffer Growth**:
- `Speech buffer size: X samples (Y seconds)`
- `Required samples for processing: X, have: Y`
- Buffer needs to reach threshold before processing

**Processing Thread**:
- `Processing thread started` - Confirms thread is running
- `Processing chunk from queue` - Shows when chunks are processed
- `Queueing audio chunk: X samples` - When audio is queued

**Whisper Processing**:
- `[WhisperWrapper] process_stream called with X samples`
- `[WhisperWrapper] Processing window of X samples`
- `[WhisperWrapper] Running whisper_full on X samples`
- `[WhisperWrapper] Number of segments: X`
- `[WhisperWrapper] Got text: 'transcription'`

### 3. Common Issues:

- **No VAD state changes**: Energy threshold too high
- **Buffer never fills**: Buffer size requirement too large
- **No Whisper output**: Audio format issue or model problem
- **Audio stats show max=0**: Microphone permission or device issue
