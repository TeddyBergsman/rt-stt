#ifndef WHISPER_WRAPPER_H
#define WHISPER_WRAPPER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>

// Forward declarations for whisper.cpp types
struct whisper_context;
struct whisper_full_params;

namespace rt_stt {
namespace stt {

// Transcription result
struct TranscriptionResult {
    std::string text;
    float confidence;
    bool is_final;
    std::chrono::milliseconds processing_time;
    std::string language;
    std::vector<std::pair<int64_t, int64_t>> timestamps; // start, end in ms
};

// Model configuration
struct ModelConfig {
    std::string model_path;
    std::string language = "en"; // "en" for English, "auto" for detection
    int n_threads = 4;
    int n_processors = 1;
    bool use_gpu = true;
    bool flash_attn = false;
    int beam_size = 5;
    float temperature = 0.0f;
    bool translate = false; // Translate to English
};

class WhisperWrapper {
public:
    using TranscriptionCallback = std::function<void(const TranscriptionResult&)>;
    
    WhisperWrapper();
    ~WhisperWrapper();
    
    // Initialize with model
    bool initialize(const ModelConfig& config);
    void shutdown();
    
    // Process audio data
    void process_audio(const float* samples, size_t n_samples, TranscriptionCallback callback);
    
    // Stream processing (for real-time)
    void process_stream(const float* samples, size_t n_samples, TranscriptionCallback callback);
    
    // Configuration
    void set_language(const std::string& language);
    void set_translate(bool translate);
    void set_beam_size(int beam_size);
    
    // Model info
    bool is_multilingual() const;
    std::vector<std::string> get_available_languages() const;
    std::string get_model_type() const;
    
    // Performance metrics
    float get_rtf() const; // Real-time factor
    size_t get_model_memory_usage() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    // Streaming state
    struct StreamingState {
        std::vector<float> context_buffer;
        std::vector<float> overlap_buffer;
        int64_t offset_ms = 0;
        std::string previous_text;
        int n_failures = 0;
    };
    
    std::unique_ptr<StreamingState> streaming_state_;
    
    // Internal methods
    void reset_streaming_state();
    TranscriptionResult process_segment(const float* samples, size_t n_samples);
    float calculate_confidence(whisper_context* ctx);
};

} // namespace stt
} // namespace rt_stt

#endif // WHISPER_WRAPPER_H
