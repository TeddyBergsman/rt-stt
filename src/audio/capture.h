#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>

// Forward declarations for Core Audio types
#ifdef __APPLE__
struct AudioTimeStamp;
struct AudioBufferList;
typedef unsigned int AudioUnitRenderActionFlags;
typedef unsigned int UInt32;
typedef int OSStatus;
#endif

namespace rt_stt {
namespace audio {

// Audio capture configuration
struct CaptureConfig {
    std::string device_name = "MOTU M2";  // Empty for default device
    int sample_rate = 16000;
    int channels = 1;
    int buffer_size_ms = 30;
    bool use_callback = true;
    int bit_depth = 32; // float32
};

// Audio device information
struct DeviceInfo {
    std::string name;
    std::string id;
    int max_input_channels;
    int max_output_channels;
    int default_sample_rate;
    bool is_default_input;
    bool is_default_output;
};

class AudioCapture {
public:
    using AudioCallback = std::function<void(const float* samples, size_t n_samples)>;
    
    // Friend function for Core Audio callback
    friend OSStatus CoreAudioCallback(void*, AudioUnitRenderActionFlags*, 
                                     const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
    
    AudioCapture();
    ~AudioCapture();
    
    // Initialize with configuration
    bool initialize(const CaptureConfig& config);
    void shutdown();
    
    // Start/stop capture
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    // Set callback for audio data
    void set_callback(AudioCallback callback) { callback_ = callback; }
    
    // Get available devices
    static std::vector<DeviceInfo> enumerate_devices();
    
    // Get current device info
    DeviceInfo get_current_device() const;
    
    // Non-callback mode: pull audio data
    size_t read_samples(float* buffer, size_t max_samples);
    
    // Get actual configuration after initialization
    CaptureConfig get_config() const { return config_; }
    
    // Internal audio callback (needs to be public for C callback)
    void process_audio_callback(const float* input, size_t frame_count);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    CaptureConfig config_;
    AudioCallback callback_;
    std::atomic<bool> running_{false};
    
    // Platform-specific initialization
    bool initialize_coreaudio();
    bool initialize_miniaudio();
};

} // namespace audio
} // namespace rt_stt

#endif // AUDIO_CAPTURE_H
