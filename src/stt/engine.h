#ifndef STT_ENGINE_H
#define STT_ENGINE_H

#include "stt/whisper_wrapper.h"
#include "audio/vad.h"
#include "utils/terminal_output.h"
#include <memory>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace rt_stt {
namespace stt {

class STTEngine {
public:
    struct Config {
        ModelConfig model_config;
        audio::VADConfig vad_config;
        bool enable_terminal_output = false;
        bool measure_performance = true;
        size_t audio_buffer_size_ms = 30;
        size_t max_queue_size = 100;
    };
    
    using TranscriptionCallback = std::function<void(const TranscriptionResult&)>;
    
    STTEngine();
    ~STTEngine();
    
    // Lifecycle
    bool initialize(const Config& config);
    void shutdown();
    
    // Control
    void start();
    void stop();
    void pause();
    void resume();
    bool is_running() const { return running_.load(); }
    bool is_paused() const { return paused_.load(); }
    
    // Audio input
    void feed_audio(const float* samples, size_t n_samples);
    
    // Callbacks
    void set_transcription_callback(TranscriptionCallback callback);
    
    // Configuration
    void set_language(const std::string& language);
    void set_vad_enabled(bool enabled);
    void update_vad_config(const audio::VADConfig& config);
    void set_model(const std::string& model_path);
    Config get_current_config() const { return config_; }
    
    // Performance metrics
    struct Metrics {
        float avg_latency_ms = 0.0f;
        float avg_rtf = 0.0f;
        float cpu_usage = 0.0f;
        size_t memory_usage_mb = 0;
        size_t processed_samples = 0;
        size_t transcriptions_count = 0;
    };
    
    Metrics get_metrics() const;
    
private:
    // Core components
    std::unique_ptr<WhisperWrapper> whisper_;
    std::unique_ptr<audio::VAD> vad_;
    std::unique_ptr<utils::TerminalOutput> terminal_output_;
    
    // State
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    Config config_;
    
    // Audio processing
    struct AudioChunk {
        std::vector<float> samples;
        std::chrono::steady_clock::time_point timestamp;
        bool is_speech_start = false;
        bool is_speech_end = false;
    };
    
    std::queue<AudioChunk> audio_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Processing thread
    std::thread processing_thread_;
    void processing_loop();
    
    // Speech buffer for VAD
    std::vector<float> speech_buffer_;
    bool in_speech_ = false;
    std::chrono::steady_clock::time_point speech_start_time_;
    
    // Callbacks
    TranscriptionCallback transcription_callback_;
    
    // Metrics tracking
    mutable std::mutex metrics_mutex_;
    Metrics metrics_;
    std::chrono::steady_clock::time_point last_metrics_update_;
    
    // Helper methods
    void process_audio_chunk(const AudioChunk& chunk);
    void handle_transcription(const TranscriptionResult& result);
    void update_metrics();
    void clear_buffers();
};

} // namespace stt
} // namespace rt_stt

#endif // STT_ENGINE_H
