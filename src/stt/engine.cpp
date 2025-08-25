#include "stt/engine.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#include <algorithm>

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#endif

namespace rt_stt {
namespace stt {

STTEngine::STTEngine() {
    whisper_ = std::make_unique<WhisperWrapper>();
    vad_ = std::make_unique<audio::VAD>();
}

STTEngine::~STTEngine() {
    shutdown();
}

bool STTEngine::initialize(const Config& config) {
    config_ = config;
    
    // Initialize terminal output if enabled
    if (config_.enable_terminal_output) {
        terminal_output_ = std::make_unique<utils::TerminalOutput>();
        terminal_output_->clear_screen();
        terminal_output_->print_status("Initializing RT-STT Engine...");
    }
    
    // Initialize Whisper
    if (!whisper_->initialize(config.model_config)) {
        if (terminal_output_) {
            terminal_output_->print_error("Failed to initialize Whisper model");
        }
        return false;
    }
    
    // Configure VAD
    vad_->update_config(config.vad_config);
    vad_->set_state_callback([this](audio::VAD::State old_state, audio::VAD::State new_state) {
        if (terminal_output_) {
            bool is_speaking = (new_state == audio::VAD::State::SPEECH || 
                               new_state == audio::VAD::State::SPEECH_ENDING);
            terminal_output_->print_vad_status(is_speaking);
            
            // Debug: show all state transitions
            if (old_state == audio::VAD::State::SPEECH_ENDING && new_state == audio::VAD::State::SILENCE) {
                terminal_output_->print_status("VAD: SPEECH_ENDING -> SILENCE transition detected");
            }
        }
        
        // Clear buffer ONLY when starting fresh from silence
        if (old_state == audio::VAD::State::SILENCE && new_state == audio::VAD::State::SPEECH_MAYBE) {
            speech_buffer_.clear();
            if (terminal_output_) {
                terminal_output_->print_status("Starting new utterance - cleared speech buffer");
            }
        }
        
        // Handle speech start - Include pre-speech buffer
        if (old_state == audio::VAD::State::SPEECH_MAYBE && new_state == audio::VAD::State::SPEECH) {
            // Get pre-speech buffer to capture the beginning
            auto pre_speech = vad_->get_buffered_audio();
            if (!pre_speech.empty()) {
                if (terminal_output_) {
                    // Calculate actual energy in pre-speech buffer for debugging
                    float max_energy = 0.0f;
                    for (size_t i = 0; i < pre_speech.size(); ++i) {
                        max_energy = std::max(max_energy, std::abs(pre_speech[i]));
                    }
                    terminal_output_->print_status("Pre-speech buffer: " + 
                                                 std::to_string(pre_speech.size() / 16000.0f) + 
                                                 " seconds, max amplitude: " + 
                                                 std::to_string(max_energy));
                }
                // Prepend pre-speech audio to our buffer
                speech_buffer_.insert(speech_buffer_.begin(), pre_speech.begin(), pre_speech.end());
            }
        }
        
        // Handle speech end - Process COMPLETE utterances only
        // Note: VAD goes SPEECH -> SPEECH_ENDING -> SILENCE
        if (old_state == audio::VAD::State::SPEECH_ENDING && new_state == audio::VAD::State::SILENCE) {
            
            // Process accumulated speech buffer
            size_t min_samples = 8000; // 0.5 seconds minimum
            if (!speech_buffer_.empty() && speech_buffer_.size() > min_samples && !paused_.load()) {
                if (terminal_output_) {
                    float duration = speech_buffer_.size() / 16000.0f;
                    terminal_output_->print_status("Processing utterance: " + 
                                                 std::to_string(duration) + " seconds");
                }
                
                AudioChunk chunk;
                chunk.samples = speech_buffer_;  // Copy instead of move for debugging
                chunk.timestamp = std::chrono::steady_clock::now();
                chunk.is_speech_end = true;
                
                // Debug: Check first 0.5 seconds of audio
                if (terminal_output_ && chunk.samples.size() > 8000) {
                    float max_in_first_half_sec = 0.0f;
                    for (size_t i = 0; i < 8000; ++i) {
                        max_in_first_half_sec = std::max(max_in_first_half_sec, std::abs(chunk.samples[i]));
                    }
                    terminal_output_->print_status("First 0.5s max amplitude: " + 
                                                 std::to_string(max_in_first_half_sec));
                }
                
                std::lock_guard<std::mutex> lock(queue_mutex_);
                audio_queue_.push(std::move(chunk));
                queue_cv_.notify_one();
                
                speech_buffer_.clear();
                
                speech_buffer_.clear();
            } else if (!speech_buffer_.empty()) {
                // Too short, discard
                if (terminal_output_) {
                    float duration = speech_buffer_.size() / 16000.0f;
                    terminal_output_->print_status("Discarding short utterance: " + 
                                                 std::to_string(duration) + " seconds (min: 0.5s)");
                }
                speech_buffer_.clear();
            }
        }
    });
    
    if (terminal_output_) {
        terminal_output_->print_status("STT Engine initialized successfully");
        terminal_output_->print_status("Model: " + whisper_->get_model_type() + 
                                     (whisper_->is_multilingual() ? " (multilingual)" : " (English)"));
    }
    
    return true;
}

void STTEngine::shutdown() {
    stop();
    whisper_->shutdown();
    
    if (terminal_output_) {
        terminal_output_->print_status("STT Engine shut down");
    }
}

void STTEngine::start() {
    if (running_.load()) return;
    
    running_ = true;
    paused_ = false;
    
    // Starting processing thread
    
    // Start processing thread
    processing_thread_ = std::thread(&STTEngine::processing_loop, this);
    
    if (terminal_output_) {
        terminal_output_->print_status("STT Engine started");
    }
}

void STTEngine::stop() {
    if (!running_.load()) return;
    
    running_ = false;
    queue_cv_.notify_all();
    
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
    
    clear_buffers();
    
    if (terminal_output_) {
        terminal_output_->print_status("STT Engine stopped");
    }
}

void STTEngine::pause() {
    paused_ = true;
    if (terminal_output_) {
        terminal_output_->print_status("STT Engine paused");
    }
}

void STTEngine::resume() {
    paused_ = false;
    queue_cv_.notify_all();
    if (terminal_output_) {
        terminal_output_->print_status("STT Engine resumed");
    }
}

void STTEngine::feed_audio(const float* samples, size_t n_samples) {
    if (!running_.load() || paused_.load()) return;
    
    // Debug: Check audio samples
    static size_t total_calls = 0;
    static size_t non_zero_calls = 0;
    total_calls++;
    
    float max_sample = 0.0f;
    float avg_sample = 0.0f;
    for (size_t i = 0; i < n_samples; ++i) {
        avg_sample += std::abs(samples[i]);
        max_sample = std::max(max_sample, std::abs(samples[i]));
    }
    avg_sample /= n_samples;
    
    if (max_sample > 0.0001f) {
        non_zero_calls++;
    }
    
    // Audio stats (debug output disabled)
    
    // Check if VAD is effectively disabled (threshold = 0)
    bool vad_disabled = config_.vad_config.energy_threshold == 0.0f;
    
    // VAD configuration applied
    
    // Run VAD
    auto vad_state = vad_->process(samples, n_samples);
    
    // Force SPEECH state if VAD is disabled
    if (vad_disabled) {
        vad_state = audio::VAD::State::SPEECH;
        // VAD is disabled, forcing speech state
    }
    
    // Update audio level display
    if (terminal_output_ && config_.enable_terminal_output) {
        terminal_output_->print_audio_level(vad_->get_current_energy());
        
        // Debug VAD state changes
        static audio::VAD::State last_vad_state = audio::VAD::State::SILENCE;
        if (vad_state != last_vad_state) {
            std::string state_str;
            switch(vad_state) {
                case audio::VAD::State::SILENCE: state_str = "SILENCE"; break;
                case audio::VAD::State::SPEECH_MAYBE: state_str = "SPEECH_MAYBE"; break;
                case audio::VAD::State::SPEECH: state_str = "SPEECH"; break;
                case audio::VAD::State::SPEECH_ENDING: state_str = "SPEECH_ENDING"; break;
            }
            terminal_output_->print_status("VAD State: " + state_str + ", Energy: " + 
                                         std::to_string(vad_->get_current_energy()) + 
                                         ", Noise floor: " + std::to_string(vad_->get_noise_floor()));
            last_vad_state = vad_state;
        }
    }
    
    // VAD state tracking (debug output disabled for cleaner display)
    
    // Handle audio based on VAD state
    if (vad_state == audio::VAD::State::SPEECH || 
        vad_state == audio::VAD::State::SPEECH_ENDING ||
        vad_state == audio::VAD::State::SPEECH_MAYBE) {
        
        // Add to speech buffer
        speech_buffer_.insert(speech_buffer_.end(), samples, samples + n_samples);
        
        // Buffer is growing...
        
        // DON'T process while speaking - just accumulate audio
        // Processing happens only when speech ends (in the VAD callback)
        in_speech_ = true;
        
        // Debug: show buffer growth periodically
        static size_t debug_counter = 0;
        if (++debug_counter % 100 == 0 && terminal_output_) {
            float duration = speech_buffer_.size() / 16000.0f;
            terminal_output_->print_status("Speech buffer: " + 
                                         std::to_string(duration) + " seconds");
        }
    } else if (vad_state == audio::VAD::State::SILENCE) {
        if (in_speech_) {
            in_speech_ = false;
        }
    }
    
    // Update metrics
    metrics_.processed_samples += n_samples;
}

void STTEngine::processing_loop() {
    // Processing thread started
    
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        // Wait for audio or shutdown
        queue_cv_.wait(lock, [this] {
            return !audio_queue_.empty() || !running_.load() || paused_.load();
        });
        
        if (!running_.load()) break;
        if (paused_.load()) continue;
        
        if (!audio_queue_.empty()) {
            size_t queue_size = audio_queue_.size();
            AudioChunk chunk = std::move(audio_queue_.front());
            audio_queue_.pop();
            lock.unlock();
            
            // Processing chunk from queue
            
            // Process the chunk
            process_audio_chunk(chunk);
        }
    }
    
    // Processing thread stopped
}

void STTEngine::process_audio_chunk(const AudioChunk& chunk) {
    auto start_time = std::chrono::steady_clock::now();
    
    // Processing audio chunk
    
    // Process with Whisper
    whisper_->process_stream(
        chunk.samples.data(), 
        chunk.samples.size(),
        [this, start_time, &chunk](const TranscriptionResult& result) {
            // Calculate latency
            auto end_time = std::chrono::steady_clock::now();
            auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - chunk.timestamp
            );
            
            // Update result with latency
            TranscriptionResult final_result = result;
            final_result.processing_time = latency;
            
            // Handle the transcription
            handle_transcription(final_result);
            
            // Update metrics
            {
                std::lock_guard<std::mutex> lock(metrics_mutex_);
                metrics_.transcriptions_count++;
                metrics_.avg_latency_ms = 
                    (metrics_.avg_latency_ms * (metrics_.transcriptions_count - 1) + 
                     latency.count()) / metrics_.transcriptions_count;
            }
        }
    );
    
    update_metrics();
}

void STTEngine::handle_transcription(const TranscriptionResult& result) {
    // Display in terminal if enabled
    if (terminal_output_) {
        terminal_output_->print_transcript(result.text, result.confidence, result.is_final);
        terminal_output_->print_latency(result.processing_time);
    }
    
    // Call user callback
    if (transcription_callback_) {
        transcription_callback_(result);
    }
}

void STTEngine::set_transcription_callback(TranscriptionCallback callback) {
    transcription_callback_ = callback;
}

void STTEngine::set_language(const std::string& language) {
    whisper_->set_language(language);
    if (terminal_output_) {
        terminal_output_->print_status("Language set to: " + language);
    }
}

void STTEngine::set_vad_enabled(bool enabled) {
    // VAD is always used internally, this would control whether we use it for segmentation
    if (terminal_output_) {
        terminal_output_->print_status("VAD " + std::string(enabled ? "enabled" : "disabled"));
    }
}

void STTEngine::update_vad_config(const audio::VADConfig& config) {
    vad_->update_config(config);
    config_.vad_config = config;
}

void STTEngine::set_model(const std::string& model_path) {
    // Stop processing during model switch
    bool was_running = running_.load();
    if (was_running) {
        stop();
    }
    
    // Update config with new model path
    config_.model_config.model_path = model_path;
    
    // Create new whisper instance
    whisper_ = std::make_unique<WhisperWrapper>();
    
    // Initialize with updated config
    if (!whisper_->initialize(config_.model_config)) {
        throw std::runtime_error("Failed to load model: " + model_path);
    }
    
    // Restart if was running
    if (was_running) {
        start();
    }
    
    if (terminal_output_) {
        terminal_output_->print_status("Model changed to: " + model_path);
    }
}

void STTEngine::update_metrics() {
    auto now = std::chrono::steady_clock::now();
    if (now - last_metrics_update_ < std::chrono::seconds(1)) {
        return;
    }
    last_metrics_update_ = now;
    
#ifdef __APPLE__
    // Get CPU usage
    processor_cpu_load_info_t cpuLoad;
    mach_msg_type_number_t processorMsgCount;
    natural_t processorCount;
    
    kern_return_t err = host_processor_info(
        mach_host_self(), 
        PROCESSOR_CPU_LOAD_INFO, 
        &processorCount,
        (processor_info_array_t*)&cpuLoad, 
        &processorMsgCount
    );
    
    if (err == KERN_SUCCESS) {
        float total_idle = 0;
        float total_ticks = 0;
        
        for (natural_t i = 0; i < processorCount; i++) {
            total_idle += cpuLoad[i].cpu_ticks[CPU_STATE_IDLE];
            total_ticks += cpuLoad[i].cpu_ticks[CPU_STATE_USER] +
                          cpuLoad[i].cpu_ticks[CPU_STATE_SYSTEM] +
                          cpuLoad[i].cpu_ticks[CPU_STATE_IDLE] +
                          cpuLoad[i].cpu_ticks[CPU_STATE_NICE];
        }
        
        float cpu_usage = 100.0f * (1.0f - total_idle / total_ticks);
        
        {
            std::lock_guard<std::mutex> lock(metrics_mutex_);
            metrics_.cpu_usage = cpu_usage;
        }
    }
    
    // Get memory usage
    struct task_basic_info info;
    mach_msg_type_number_t size = TASK_BASIC_INFO_COUNT;
    
    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &size) == KERN_SUCCESS) {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.memory_usage_mb = info.resident_size / (1024 * 1024);
    }
#endif
    
    // Update RTF
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        metrics_.avg_rtf = whisper_->get_rtf();
    }
    
    // Display metrics
    if (terminal_output_ && config_.measure_performance) {
        terminal_output_->update_metrics(
            metrics_.cpu_usage,
            metrics_.memory_usage_mb,
            1 // Active threads (simplified)
        );
    }
}

STTEngine::Metrics STTEngine::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    return metrics_;
}

void STTEngine::clear_buffers() {
    speech_buffer_.clear();
    in_speech_ = false;
    
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!audio_queue_.empty()) {
        audio_queue_.pop();
    }
    
    vad_->reset();
}

} // namespace stt
} // namespace rt_stt
