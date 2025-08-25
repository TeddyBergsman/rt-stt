# RT-STT Full Metadata Output

## What's New

The JSON output now includes **ALL available metadata** from the Whisper model, not just basic text and confidence. This provides deep insights into the transcription process without adding any noticeable latency.

## Complete JSON Structure

When streaming with `-j` flag, you'll now see:

```json
{
  "data": {
    "text": "Hello world",
    "confidence": 0.95,
    "timestamp": 1756156076930913,
    "language": "en",
    "language_probability": 0.99,
    "processing_time_ms": 187,
    "audio_duration_ms": 1500,
    "model": "ggml-large-v3.bin",
    "is_final": true,
    "segments": [
      {
        "id": 0,
        "seek": 0,
        "start": 0.0,
        "end": 1.5,
        "text": " Hello world",
        "tokens": [50364, 2425, 1002, 50414],
        "temperature": 0.0,
        "avg_logprob": -0.123,
        "compression_ratio": 1.0,
        "no_speech_prob": 0.0
      }
    ]
  },
  "id": "1756156076930911",
  "type": 3
}
```

## Field Descriptions

### Top-Level Fields
- **text**: Complete transcribed text
- **confidence**: Overall confidence score (0-1)
- **timestamp**: Unix timestamp in microseconds
- **language**: Detected/specified language code
- **language_probability**: Confidence in language detection (auto mode)
- **processing_time_ms**: Time taken to process audio
- **audio_duration_ms**: Duration of audio processed
- **model**: Model filename being used
- **is_final**: Whether this is a complete utterance

### Segment Fields
Each segment represents a portion of the transcription:
- **id**: Segment number
- **seek**: Seek position (internal use)
- **start/end**: Timing in seconds
- **text**: Text for this segment
- **tokens**: Whisper token IDs
- **temperature**: Sampling temperature used
- **avg_logprob**: Average log probability (confidence metric)
- **compression_ratio**: Text compression metric
- **no_speech_prob**: Probability of no speech

## Performance Impact

**None!** All this data is already computed by Whisper during transcription. We're just extracting and passing it through instead of discarding it.

## Usage Examples

### View full output:
```bash
rt-stt-cli stream -j -s /tmp/rt-stt-test.sock | jq '.'
```

### Extract specific fields:
```bash
# Just text and confidence
rt-stt-cli stream -j -s /tmp/rt-stt-test.sock | jq '.data | {text, confidence}'

# Performance metrics
rt-stt-cli stream -j -s /tmp/rt-stt-test.sock | jq '.data | {processing_time_ms, audio_duration_ms}'

# Language detection info
rt-stt-cli stream -j -s /tmp/rt-stt-test.sock | jq '.data | {text, language, language_probability}'
```

### Save for analysis:
```bash
# Record session with full metadata
rt-stt-cli stream -j -s /tmp/rt-stt-test.sock > session_full.jsonl

# Analyze token patterns
cat session_full.jsonl | jq -r '.data.segments[].tokens | @csv'

# Check confidence over time
cat session_full.jsonl | jq '{time: .data.timestamp, confidence: .data.confidence, text: .data.text}'
```

## What This Enables

1. **Quality Analysis**: Track confidence scores and probabilities
2. **Performance Monitoring**: See actual processing times vs audio duration
3. **Language Detection**: Understand how confident the model is about languages
4. **Token Analysis**: Debug or analyze what tokens Whisper is producing
5. **Timing Information**: Precise segment timing for subtitles/captions
6. **Model Comparison**: Compare different models' outputs in detail
