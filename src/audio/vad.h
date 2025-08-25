#ifndef VAD_H
#define VAD_H

#include <vector>
#include <deque>
#include <cstddef>
#include <functional>

namespace rt_stt {
namespace audio {

// VAD configuration
struct VADConfig {
    // Energy-based VAD parameters
    float energy_threshold = 0.01f;
    float speech_start_threshold = 0.02f;
    float speech_end_threshold = 0.01f;
    
    // Timing parameters (in milliseconds)
    int speech_start_ms = 200;      // Min speech duration to trigger
    int speech_end_ms = 500;        // Silence duration to end speech
    int min_speech_ms = 100;        // Minimum speech duration to keep
    int pre_speech_buffer_ms = 300; // Audio to keep before speech starts
    
    // Advanced parameters
    bool use_adaptive_threshold = true;
    float noise_floor_adaptation_rate = 0.001f;
    int sample_rate = 16000;
};

// Simple energy-based VAD with adaptive threshold
class VAD {
public:
    enum class State {
        SILENCE,
        SPEECH_MAYBE,  // Potential speech start
        SPEECH,        // Confirmed speech
        SPEECH_ENDING  // Potential speech end
    };
    
    using StateCallback = std::function<void(State old_state, State new_state)>;
    
    VAD(const VADConfig& config = VADConfig());
    ~VAD() = default;
    
    // Process audio samples and return current state
    State process(const float* samples, size_t n_samples);
    
    // Get current state
    State get_state() const { return state_; }
    
    // Get buffered audio (useful for pre-speech buffer)
    std::vector<float> get_buffered_audio() const;
    
    // Configuration
    void update_config(const VADConfig& config);
    VADConfig get_config() const { return config_; }
    
    // State change callback
    void set_state_callback(StateCallback callback) { state_callback_ = callback; }
    
    // Reset VAD state
    void reset();
    
    // Get current energy level (for visualization)
    float get_current_energy() const { return current_energy_; }
    float get_noise_floor() const { return noise_floor_; }
    
private:
    VADConfig config_;
    State state_;
    StateCallback state_callback_;
    
    // Audio buffer for pre-speech
    std::deque<float> audio_buffer_;
    size_t buffer_max_samples_;
    
    // Energy tracking
    float current_energy_;
    float noise_floor_;
    std::vector<float> energy_history_;
    size_t energy_history_idx_;
    
    // State timing
    size_t speech_frames_;
    size_t silence_frames_;
    size_t frames_per_ms_;
    
    // Internal methods
    float calculate_energy(const float* samples, size_t n_samples);
    void update_noise_floor(float energy);
    void change_state(State new_state);
    void update_buffer(const float* samples, size_t n_samples);
};

// Advanced VAD using spectral features (optional, for future enhancement)
class SpectralVAD : public VAD {
public:
    SpectralVAD(const VADConfig& config = VADConfig());
    
    // Override with spectral analysis
    State process(const float* samples, size_t n_samples);
    
private:
    // FFT and spectral analysis members
    std::vector<float> fft_buffer_;
    std::vector<float> spectral_features_;
    
    float calculate_spectral_flatness(const float* spectrum, size_t n_bins);
    float calculate_spectral_entropy(const float* spectrum, size_t n_bins);
};

} // namespace audio
} // namespace rt_stt

#endif // VAD_H
