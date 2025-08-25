#include "audio/vad.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace rt_stt {
namespace audio {

VAD::VAD(const VADConfig& config)
    : config_(config)
    , state_(State::SILENCE)
    , current_energy_(0.0f)
    , noise_floor_(config.energy_threshold)
    , speech_frames_(0)
    , silence_frames_(0)
    , energy_history_idx_(0) {
    
    // Calculate frame counts
    frames_per_ms_ = config_.sample_rate / 1000;
    
    // Initialize buffers
    buffer_max_samples_ = (config_.pre_speech_buffer_ms * config_.sample_rate) / 1000;
    energy_history_.resize(100, config_.energy_threshold);
}

VAD::State VAD::process(const float* samples, size_t n_samples) {
    // Calculate frame energy
    current_energy_ = calculate_energy(samples, n_samples);
    
    // Update noise floor if adaptive
    if (config_.use_adaptive_threshold && state_ == State::SILENCE) {
        update_noise_floor(current_energy_);
    }
    
    // Update audio buffer
    update_buffer(samples, n_samples);
    
    // Determine thresholds
    // Fixed calculation - speech_start_threshold is a multiplier of noise floor
    float speech_threshold = config_.use_adaptive_threshold 
        ? noise_floor_ * config_.speech_start_threshold
        : config_.speech_start_threshold;
    
    // Debug: print thresholds periodically (disabled for cleaner output)
    // static size_t debug_counter = 0;
    // if (++debug_counter % 100 == 0) {
    //     printf("[VAD] Energy: %.6f, Speech threshold: %.6f, Noise floor: %.6f\n", 
    //            current_energy_, speech_threshold, noise_floor_);
    // }
    
    float silence_threshold = config_.use_adaptive_threshold
        ? noise_floor_ * config_.speech_end_threshold
        : config_.speech_end_threshold;
    
    // State machine
    State old_state = state_;
    
    switch (state_) {
        case State::SILENCE:
            if (current_energy_ > speech_threshold) {
                change_state(State::SPEECH_MAYBE);
                speech_frames_ = n_samples;
                silence_frames_ = 0;
            }
            break;
            
        case State::SPEECH_MAYBE:
            if (current_energy_ > speech_threshold) {
                speech_frames_ += n_samples;
                if (speech_frames_ >= config_.speech_start_ms * frames_per_ms_) {
                    change_state(State::SPEECH);
                }
            } else {
                // False start, back to silence
                change_state(State::SILENCE);
                speech_frames_ = 0;
            }
            break;
            
        case State::SPEECH:
            if (current_energy_ < silence_threshold) {
                change_state(State::SPEECH_ENDING);
                silence_frames_ = n_samples;
            } else {
                speech_frames_ += n_samples;
            }
            break;
            
        case State::SPEECH_ENDING:
            if (current_energy_ < silence_threshold) {
                silence_frames_ += n_samples;
                if (silence_frames_ >= config_.speech_end_ms * frames_per_ms_) {
                    // Check if speech was long enough
                    if (speech_frames_ >= config_.min_speech_ms * frames_per_ms_) {
                        change_state(State::SILENCE);
                    } else {
                        // Too short, treat as noise
                        change_state(State::SILENCE);
                    }
                    speech_frames_ = 0;
                    silence_frames_ = 0;
                }
            } else {
                // Speech resumed
                change_state(State::SPEECH);
                silence_frames_ = 0;
            }
            break;
    }
    
    return state_;
}

float VAD::calculate_energy(const float* samples, size_t n_samples) {
    if (n_samples == 0) return 0.0f;
    
    // Calculate RMS energy
    float sum_squares = 0.0f;
    for (size_t i = 0; i < n_samples; ++i) {
        sum_squares += samples[i] * samples[i];
    }
    
    return std::sqrt(sum_squares / n_samples);
}

void VAD::update_noise_floor(float energy) {
    // Adaptive noise floor estimation
    energy_history_[energy_history_idx_] = energy;
    energy_history_idx_ = (energy_history_idx_ + 1) % energy_history_.size();
    
    // Use percentile method for noise floor
    std::vector<float> sorted_history = energy_history_;
    std::sort(sorted_history.begin(), sorted_history.end());
    
    // Use 20th percentile as noise floor
    size_t percentile_idx = sorted_history.size() / 5;
    float new_noise_floor = sorted_history[percentile_idx];
    
    // Smooth adaptation
    noise_floor_ = noise_floor_ * (1.0f - config_.noise_floor_adaptation_rate) +
                   new_noise_floor * config_.noise_floor_adaptation_rate;
    
    // Ensure minimum threshold
    noise_floor_ = std::max(noise_floor_, config_.energy_threshold * 0.5f);
}

void VAD::change_state(State new_state) {
    if (state_ != new_state) {
        State old_state = state_;
        state_ = new_state;
        
        if (state_callback_) {
            state_callback_(old_state, new_state);
        }
    }
}

void VAD::update_buffer(const float* samples, size_t n_samples) {
    // Add new samples to buffer
    for (size_t i = 0; i < n_samples; ++i) {
        audio_buffer_.push_back(samples[i]);
    }
    
    // Keep buffer size limited
    while (audio_buffer_.size() > buffer_max_samples_) {
        audio_buffer_.pop_front();
    }
}

std::vector<float> VAD::get_buffered_audio() const {
    return std::vector<float>(audio_buffer_.begin(), audio_buffer_.end());
}

void VAD::update_config(const VADConfig& config) {
    config_ = config;
    frames_per_ms_ = config_.sample_rate / 1000;
    buffer_max_samples_ = (config_.pre_speech_buffer_ms * config_.sample_rate) / 1000;
    
    // Reset adaptive parameters
    if (config_.use_adaptive_threshold) {
        noise_floor_ = config_.energy_threshold;
        energy_history_.assign(100, config_.energy_threshold);
        energy_history_idx_ = 0;
    }
}

void VAD::reset() {
    state_ = State::SILENCE;
    speech_frames_ = 0;
    silence_frames_ = 0;
    audio_buffer_.clear();
    current_energy_ = 0.0f;
    
    if (config_.use_adaptive_threshold) {
        noise_floor_ = config_.energy_threshold;
        energy_history_.assign(energy_history_.size(), config_.energy_threshold);
        energy_history_idx_ = 0;
    }
}

// SpectralVAD implementation (stub for now)
SpectralVAD::SpectralVAD(const VADConfig& config) : VAD(config) {
    // Initialize FFT buffers
    fft_buffer_.resize(1024);
    spectral_features_.resize(4); // Flatness, entropy, centroid, rolloff
}

VAD::State SpectralVAD::process(const float* samples, size_t n_samples) {
    // For now, fall back to energy-based VAD
    // TODO: Implement spectral analysis
    return VAD::process(samples, n_samples);
}

float SpectralVAD::calculate_spectral_flatness(const float* spectrum, size_t n_bins) {
    // Geometric mean / arithmetic mean
    float log_sum = 0.0f;
    float sum = 0.0f;
    
    for (size_t i = 0; i < n_bins; ++i) {
        float magnitude = std::abs(spectrum[i]);
        log_sum += std::log(magnitude + 1e-10f);
        sum += magnitude;
    }
    
    float geometric_mean = std::exp(log_sum / n_bins);
    float arithmetic_mean = sum / n_bins;
    
    return geometric_mean / (arithmetic_mean + 1e-10f);
}

float SpectralVAD::calculate_spectral_entropy(const float* spectrum, size_t n_bins) {
    // Calculate spectral probability distribution
    float sum = 0.0f;
    for (size_t i = 0; i < n_bins; ++i) {
        sum += std::abs(spectrum[i]);
    }
    
    if (sum < 1e-10f) return 0.0f;
    
    // Calculate entropy
    float entropy = 0.0f;
    for (size_t i = 0; i < n_bins; ++i) {
        float p = std::abs(spectrum[i]) / sum;
        if (p > 1e-10f) {
            entropy -= p * std::log2(p);
        }
    }
    
    return entropy;
}

} // namespace audio
} // namespace rt_stt
