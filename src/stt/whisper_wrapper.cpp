#include "stt/whisper_wrapper.h"
#include "whisper.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <numeric>
#include <cctype>

namespace rt_stt {
namespace stt {

// Constants for streaming
static constexpr int SAMPLE_RATE = 16000;
static constexpr float STREAMING_WINDOW_SEC = 5.0f;  // Larger window for better context
static constexpr float STREAMING_STEP_SEC = 1.0f;    // Larger steps to reduce processing
static constexpr float STREAMING_OVERLAP_SEC = 1.0f;

struct WhisperWrapper::Impl {
    whisper_context* ctx = nullptr;
    whisper_full_params params;
    ModelConfig config;
    std::chrono::steady_clock::time_point last_process_time;
    float total_rtf = 0.0f;
    int rtf_count = 0;
};

WhisperWrapper::WhisperWrapper() 
    : impl_(std::make_unique<Impl>())
    , streaming_state_(std::make_unique<StreamingState>()) {
}

WhisperWrapper::~WhisperWrapper() {
    shutdown();
}

bool WhisperWrapper::initialize(const ModelConfig& config) {
    impl_->config = config;
    
    // Load model with parameters
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = config.use_gpu;
    cparams.flash_attn = config.flash_attn;
    
    impl_->ctx = whisper_init_from_file_with_params(config.model_path.c_str(), cparams);
    if (!impl_->ctx) {
        std::cerr << "Failed to load model from: " << config.model_path << std::endl;
        return false;
    }
    
    // Initialize parameters for streaming
    impl_->params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    
    // Streaming-optimized parameters
    impl_->params.n_threads = config.n_threads;
    impl_->params.n_max_text_ctx = 16384;
    impl_->params.translate = config.translate;
    impl_->params.language = config.language == "auto" ? nullptr : config.language.c_str();
    impl_->params.print_special = false;
    impl_->params.print_progress = false;
    impl_->params.print_realtime = false;
    impl_->params.print_timestamps = false;
    impl_->params.single_segment = false; // Allow multiple segments
    impl_->params.max_tokens = 64;        // More tokens for better results
    impl_->params.audio_ctx = 1500;       // Full context for accuracy
    
    // Beam search parameters
    if (config.beam_size > 1) {
        impl_->params.strategy = WHISPER_SAMPLING_BEAM_SEARCH;
        impl_->params.beam_search.beam_size = config.beam_size;
    }
    
    // Temperature for sampling
    impl_->params.temperature = config.temperature;
    
    // Enable token timestamps for word-level timing
    impl_->params.token_timestamps = true;
    impl_->params.max_len = 0;  // No artificial length limit
    
    // Speed optimizations
    impl_->params.suppress_blank = true;
    impl_->params.prompt_tokens = nullptr;
    impl_->params.prompt_n_tokens = 0;
    
    std::cout << "Whisper model loaded successfully: " << get_model_type() << std::endl;
    std::cout << "Multilingual: " << (is_multilingual() ? "Yes" : "No") << std::endl;
    
    return true;
}

void WhisperWrapper::shutdown() {
    if (impl_->ctx) {
        whisper_free(impl_->ctx);
        impl_->ctx = nullptr;
    }
}

void WhisperWrapper::process_audio(const float* samples, size_t n_samples, TranscriptionCallback callback) {
    if (!impl_->ctx) return;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Process full audio
    int result = whisper_full(impl_->ctx, impl_->params, samples, n_samples);
    
    if (result != 0) {
        std::cerr << "Whisper processing failed with code: " << result << std::endl;
        return;
    }
    
    // Extract results
    const int n_segments = whisper_full_n_segments(impl_->ctx);
    
    for (int i = 0; i < n_segments; ++i) {
        TranscriptionResult tr;
        tr.text = whisper_full_get_segment_text(impl_->ctx, i);
        tr.confidence = calculate_confidence(impl_->ctx);
        tr.is_final = true;
        
        // Get timestamps
        int64_t t0 = whisper_full_get_segment_t0(impl_->ctx, i);
        int64_t t1 = whisper_full_get_segment_t1(impl_->ctx, i);
        tr.timestamps.push_back({t0 * 10, t1 * 10}); // Convert to ms
        
        // Language detection
        if (impl_->config.language == "auto") {
            int lang_id = whisper_full_lang_id(impl_->ctx);
            tr.language = whisper_lang_str(lang_id);
        } else {
            tr.language = impl_->config.language;
        }
        
        auto end_time = std::chrono::steady_clock::now();
        tr.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // Update RTF
        float audio_duration = static_cast<float>(n_samples) / SAMPLE_RATE;
        float process_duration = tr.processing_time.count() / 1000.0f;
        float rtf = process_duration / audio_duration;
        impl_->total_rtf += rtf;
        impl_->rtf_count++;
        
        callback(tr);
    }
}

void WhisperWrapper::process_stream(const float* samples, size_t n_samples, TranscriptionCallback callback) {
    if (!impl_->ctx || !samples || n_samples == 0) return;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Process the COMPLETE utterance in one go
    // No sliding windows, no repetition
    TranscriptionResult result = process_segment(samples, n_samples);
    
    // Check if we got valid text
    if (!result.text.empty()) {
        // Clean up the text
        std::string cleaned = result.text;
        
        // Remove multiple spaces
        size_t pos = 0;
        while ((pos = cleaned.find("  ", pos)) != std::string::npos) {
            cleaned.replace(pos, 2, " ");
        }
        
        // Trim leading/trailing whitespace
        size_t first = cleaned.find_first_not_of(" \t\n\r");
        size_t last = cleaned.find_last_not_of(" \t\n\r");
        if (first != std::string::npos && last != std::string::npos) {
            cleaned = cleaned.substr(first, last - first + 1);
        }
        
        // Skip if it's just punctuation
        bool has_alphanumeric = false;
        for (char c : cleaned) {
            if (std::isalnum(c)) {
                has_alphanumeric = true;
                break;
            }
        }
        
        if (has_alphanumeric && cleaned.length() > 1) {
            result.text = cleaned;
            result.is_final = true; // Complete utterance
            
            auto end_time = std::chrono::steady_clock::now();
            result.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            callback(result);
        }
    }
}

TranscriptionResult WhisperWrapper::process_segment(const float* samples, size_t n_samples) {
    TranscriptionResult result;
    
    // std::cout << "[WhisperWrapper] Running whisper_full on " << n_samples << " samples" << std::endl;
    
    // Run whisper on segment
    int ret = whisper_full(impl_->ctx, impl_->params, samples, n_samples);
    
    // std::cout << "[WhisperWrapper] whisper_full returned: " << ret << std::endl;
    
    if (ret == 0) {
        const int n_segments = whisper_full_n_segments(impl_->ctx);
        // std::cout << "[WhisperWrapper] Number of segments: " << n_segments << std::endl;
        
        if (n_segments > 0) {
            // Concatenate all segments for better results
            result.text = "";
            for (int i = 0; i < n_segments; ++i) {
                const char* text = whisper_full_get_segment_text(impl_->ctx, i);
                if (text && strlen(text) > 0) {
                    result.text += text;
                }
            }
            result.confidence = calculate_confidence(impl_->ctx);
            
            // std::cout << "[WhisperWrapper] Got text: '" << result.text << "'" << std::endl;
            
            // Trim whitespace
            result.text.erase(0, result.text.find_first_not_of(" \t\n\r"));
            result.text.erase(result.text.find_last_not_of(" \t\n\r") + 1);
            
            // Get language
            if (impl_->config.language == "auto") {
                int lang_id = whisper_full_lang_id(impl_->ctx);
                result.language = whisper_lang_str(lang_id);
            } else {
                result.language = impl_->config.language;
            }
        }
    } else {
        // std::cout << "[WhisperWrapper] ERROR: whisper_full failed with code " << ret << std::endl;
    }
    
    return result;
}

float WhisperWrapper::calculate_confidence(whisper_context* ctx) {
    // Calculate confidence based on token probabilities
    const int n_segments = whisper_full_n_segments(ctx);
    if (n_segments == 0) return 0.0f;
    
    float total_logprob = 0.0f;
    int total_tokens = 0;
    
    for (int i = 0; i < n_segments; ++i) {
        const int n_tokens = whisper_full_n_tokens(ctx, i);
        for (int j = 0; j < n_tokens; ++j) {
            auto token_data = whisper_full_get_token_data(ctx, i, j);
            total_logprob += token_data.p;
            total_tokens++;
        }
    }
    
    if (total_tokens == 0) return 0.0f;
    
    // Convert average log probability to confidence score
    float avg_logprob = total_logprob / total_tokens;
    float confidence = std::exp(avg_logprob);
    
    return std::min(1.0f, std::max(0.0f, confidence));
}

void WhisperWrapper::set_language(const std::string& language) {
    impl_->config.language = language;
    impl_->params.language = language == "auto" ? nullptr : language.c_str();
}

void WhisperWrapper::set_translate(bool translate) {
    impl_->config.translate = translate;
    impl_->params.translate = translate;
}

void WhisperWrapper::set_beam_size(int beam_size) {
    impl_->config.beam_size = beam_size;
    if (beam_size > 1) {
        impl_->params.strategy = WHISPER_SAMPLING_BEAM_SEARCH;
        impl_->params.beam_search.beam_size = beam_size;
    } else {
        impl_->params.strategy = WHISPER_SAMPLING_GREEDY;
    }
}

bool WhisperWrapper::is_multilingual() const {
    return impl_->ctx ? whisper_is_multilingual(impl_->ctx) : false;
}

std::vector<std::string> WhisperWrapper::get_available_languages() const {
    std::vector<std::string> languages;
    if (!impl_->ctx || !is_multilingual()) {
        languages.push_back("en");
        return languages;
    }
    
    // Get all supported languages
    for (int i = 0; i < whisper_lang_max_id(); ++i) {
        const char* lang = whisper_lang_str(i);
        if (lang) {
            languages.push_back(lang);
        }
    }
    
    return languages;
}

std::string WhisperWrapper::get_model_type() const {
    if (!impl_->ctx) return "unknown";
    
    // Infer model type from parameters
    int n_mels = whisper_model_n_mels(impl_->ctx);
    int n_audio_layer = whisper_model_n_audio_layer(impl_->ctx);
    
    if (n_audio_layer == 4) return "tiny";
    if (n_audio_layer == 6) return "base";
    if (n_audio_layer == 12) return "small";
    if (n_audio_layer == 24) return "medium";
    if (n_audio_layer == 32) return "large";
    
    return "custom";
}

float WhisperWrapper::get_rtf() const {
    if (impl_->rtf_count == 0) return 0.0f;
    return impl_->total_rtf / impl_->rtf_count;
}

size_t WhisperWrapper::get_model_memory_usage() const {
    if (!impl_->ctx) return 0;
    // This function no longer exists in newer whisper.cpp
    // Return an estimate based on model type
    std::string model = get_model_type();
    if (model == "tiny") return 39 * 1024 * 1024;
    if (model == "base") return 142 * 1024 * 1024;
    if (model == "small") return 466 * 1024 * 1024;
    if (model == "medium") return 1500 * 1024 * 1024;
    if (model == "large") return static_cast<size_t>(3100) * 1024 * 1024;
    return 500 * 1024 * 1024; // default estimate
}

void WhisperWrapper::reset_streaming_state() {
    streaming_state_->context_buffer.clear();
    streaming_state_->overlap_buffer.clear();
    streaming_state_->offset_ms = 0;
    streaming_state_->previous_text.clear();
    streaming_state_->n_failures = 0;
}

} // namespace stt
} // namespace rt_stt
