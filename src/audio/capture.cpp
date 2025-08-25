#include "audio/capture.h"
#include <iostream>
#include <cstring>
#include <algorithm>

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreServices/CoreServices.h>
#endif

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace rt_stt {
namespace audio {

struct AudioCapture::Impl {
#ifdef __APPLE__
    AudioUnit audio_unit = nullptr;
    AudioDeviceID device_id = kAudioDeviceUnknown;
#endif
    
    // miniaudio fallback
    ma_device ma_device;
    bool use_miniaudio = false;
    
    // Ring buffer for thread safety
    std::vector<float> ring_buffer;
    size_t write_pos = 0;
    size_t read_pos = 0;
    std::mutex buffer_mutex;
    
    // Actual channel count being captured
    int actual_channels = 1;
};

// Platform-specific audio callback for Core Audio
#ifdef __APPLE__
OSStatus CoreAudioCallback(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData
) {
    AudioCapture* capture = static_cast<AudioCapture*>(inRefCon);
    
    // Allocate buffer list for input
    int channel_count = capture->impl_->actual_channels;
    AudioBufferList* bufferList = (AudioBufferList*)malloc(
        sizeof(AudioBufferList) + sizeof(AudioBuffer) * (channel_count - 1)
    );
    
    bufferList->mNumberBuffers = channel_count;
    for (UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
        bufferList->mBuffers[i].mNumberChannels = 1;
        bufferList->mBuffers[i].mDataByteSize = inNumberFrames * sizeof(float);
        bufferList->mBuffers[i].mData = malloc(bufferList->mBuffers[i].mDataByteSize);
    }
    
    // Render audio from input
    OSStatus status = AudioUnitRender(
        capture->impl_->audio_unit,
        ioActionFlags,
        inTimeStamp,
        inBusNumber,
        inNumberFrames,
        bufferList
    );
    
    if (status == noErr) {
        // Convert to mono if needed and process
        std::vector<float> mono_buffer(inNumberFrames);
        
        if (capture->get_config().force_single_channel) {
            // Use only the specified input channel
            UInt32 channel_idx = capture->get_config().input_channel_index;
            if (channel_idx < bufferList->mNumberBuffers) {
                memcpy(mono_buffer.data(), bufferList->mBuffers[channel_idx].mData, 
                       inNumberFrames * sizeof(float));
            } else {
                // Fallback to first channel if specified channel doesn't exist
                std::cerr << "Warning: Requested channel " << channel_idx 
                         << " not available, using channel 0" << std::endl;
                memcpy(mono_buffer.data(), bufferList->mBuffers[0].mData, 
                       inNumberFrames * sizeof(float));
            }
        } else if (capture->get_config().channels == 1 && bufferList->mNumberBuffers == 1) {
            // Already mono
            memcpy(mono_buffer.data(), bufferList->mBuffers[0].mData, 
                   inNumberFrames * sizeof(float));
        } else {
            // Mix all channels to mono
            for (UInt32 frame = 0; frame < inNumberFrames; ++frame) {
                float sum = 0.0f;
                for (UInt32 ch = 0; ch < bufferList->mNumberBuffers; ++ch) {
                    float* channel_data = (float*)bufferList->mBuffers[ch].mData;
                    sum += channel_data[frame];
                }
                mono_buffer[frame] = sum / bufferList->mNumberBuffers;
            }
        }
        
        capture->process_audio_callback(mono_buffer.data(), inNumberFrames);
    }
    
    // Clean up
    for (UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
        free(bufferList->mBuffers[i].mData);
    }
    free(bufferList);
    
    return status;
}

static AudioDeviceID FindAudioDevice(const std::string& device_name) {
    if (device_name.empty()) {
        // Get default input device
        AudioDeviceID device_id;
        UInt32 size = sizeof(device_id);
        AudioObjectPropertyAddress addr = {
            kAudioHardwarePropertyDefaultInputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        
        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, 
                                     &size, &device_id) == noErr) {
            return device_id;
        }
        return kAudioDeviceUnknown;
    }
    
    // List all devices
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    
    UInt32 size;
    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &size);
    
    std::vector<AudioDeviceID> devices(size / sizeof(AudioDeviceID));
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, devices.data());
    
    // Find device by name
    for (AudioDeviceID device_id : devices) {
        // Get device name
        CFStringRef name_ref;
        UInt32 name_size = sizeof(name_ref);
        AudioObjectPropertyAddress name_addr = {
            kAudioDevicePropertyDeviceNameCFString,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        
        if (AudioObjectGetPropertyData(device_id, &name_addr, 0, nullptr, 
                                     &name_size, &name_ref) == noErr) {
            char name_buf[256];
            CFStringGetCString(name_ref, name_buf, sizeof(name_buf), kCFStringEncodingUTF8);
            CFRelease(name_ref);
            
            if (std::string(name_buf).find(device_name) != std::string::npos) {
                // Check if it has input channels
                AudioObjectPropertyAddress input_addr = {
                    kAudioDevicePropertyStreamConfiguration,
                    kAudioDevicePropertyScopeInput,
                    kAudioObjectPropertyElementMain
                };
                
                AudioObjectGetPropertyDataSize(device_id, &input_addr, 0, nullptr, &size);
                if (size > sizeof(AudioBufferList)) {
                    return device_id;
                }
            }
        }
    }
    
    return kAudioDeviceUnknown;
}
#endif

// miniaudio callback
static void MiniaudioCallback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    AudioCapture* capture = static_cast<AudioCapture*>(device->pUserData);
    if (input) {
        const float* input_buffer = static_cast<const float*>(input);
        
        if (capture->get_config().force_single_channel && device->capture.channels > 1) {
            // Extract only the specified channel
            std::vector<float> mono_buffer(frame_count);
            int channel_idx = capture->get_config().input_channel_index;
            
            // Ensure channel index is valid
            if (channel_idx >= device->capture.channels) {
                channel_idx = 0; // Fallback to first channel
            }
            
            // Extract single channel (interleaved audio format)
            for (ma_uint32 i = 0; i < frame_count; ++i) {
                mono_buffer[i] = input_buffer[i * device->capture.channels + channel_idx];
            }
            
            capture->process_audio_callback(mono_buffer.data(), frame_count);
        } else {
            // Pass through as-is (already mono or mixing all channels)
            capture->process_audio_callback(input_buffer, frame_count);
        }
    }
}

AudioCapture::AudioCapture() : impl_(std::make_unique<Impl>()) {}

AudioCapture::~AudioCapture() {
    shutdown();
}

bool AudioCapture::initialize(const CaptureConfig& config) {
    config_ = config;
    
#ifdef __APPLE__
    // Try Core Audio first
    if (initialize_coreaudio()) {
        impl_->use_miniaudio = false;
        return true;
    }
    
    std::cerr << "Core Audio initialization failed, falling back to miniaudio" << std::endl;
#endif
    
    // Fall back to miniaudio
    return initialize_miniaudio();
}

bool AudioCapture::initialize_coreaudio() {
#ifdef __APPLE__
    // Find audio device
    impl_->device_id = FindAudioDevice(config_.device_name);
    if (impl_->device_id == kAudioDeviceUnknown) {
        std::cerr << "Audio device not found: " << config_.device_name << std::endl;
        return false;
    }
    
    // Create audio unit
    AudioComponentDescription desc = {};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    
    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) {
        std::cerr << "Failed to find audio component" << std::endl;
        return false;
    }
    
    OSStatus status = AudioComponentInstanceNew(comp, &impl_->audio_unit);
    if (status != noErr) {
        std::cerr << "Failed to create audio unit: " << status << std::endl;
        return false;
    }
    
    // Enable input
    UInt32 enable_input = 1;
    status = AudioUnitSetProperty(
        impl_->audio_unit,
        kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Input,
        1, // input element
        &enable_input,
        sizeof(enable_input)
    );
    
    // Disable output
    UInt32 disable_output = 0;
    AudioUnitSetProperty(
        impl_->audio_unit,
        kAudioOutputUnitProperty_EnableIO,
        kAudioUnitScope_Output,
        0, // output element
        &disable_output,
        sizeof(disable_output)
    );
    
    // Set device
    status = AudioUnitSetProperty(
        impl_->audio_unit,
        kAudioOutputUnitProperty_CurrentDevice,
        kAudioUnitScope_Global,
        0,
        &impl_->device_id,
        sizeof(impl_->device_id)
    );
    
    if (status != noErr) {
        std::cerr << "Failed to set audio device: " << status << std::endl;
        AudioComponentInstanceDispose(impl_->audio_unit);
        impl_->audio_unit = nullptr;
        return false;
    }
    
    // Set format
    AudioStreamBasicDescription format = {};
    format.mSampleRate = config_.sample_rate;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    format.mFramesPerPacket = 1;
    
    // If forcing single channel, first get the device's actual channel count
    if (config_.force_single_channel) {
        // Get device's native format to know how many channels it has
        AudioObjectPropertyAddress prop_addr = {
            kAudioDevicePropertyStreamFormat,
            kAudioDevicePropertyScopeInput,
            kAudioObjectPropertyElementMain
        };
        
        AudioStreamBasicDescription device_format;
        UInt32 prop_size = sizeof(device_format);
        if (AudioObjectGetPropertyData(impl_->device_id, &prop_addr, 0, nullptr,
                                     &prop_size, &device_format) == noErr) {
            format.mChannelsPerFrame = device_format.mChannelsPerFrame;
            impl_->actual_channels = format.mChannelsPerFrame;
            std::cout << "Device has " << format.mChannelsPerFrame << " input channels" << std::endl;
            std::cout << "Will use channel " << (config_.input_channel_index + 1) 
                     << " (Input " << (config_.input_channel_index + 1) << ")" << std::endl;
        } else {
            // Fallback to stereo if we can't query
            format.mChannelsPerFrame = 2;
            impl_->actual_channels = 2;
        }
    } else {
        format.mChannelsPerFrame = config_.channels;
        impl_->actual_channels = config_.channels;
    }
    
    format.mBitsPerChannel = 32;
    format.mBytesPerPacket = format.mBytesPerFrame = 
        format.mChannelsPerFrame * sizeof(float);
    
    status = AudioUnitSetProperty(
        impl_->audio_unit,
        kAudioUnitProperty_StreamFormat,
        kAudioUnitScope_Output,
        1, // input element
        &format,
        sizeof(format)
    );
    
    if (status != noErr) {
        std::cerr << "Failed to set audio format: " << status << std::endl;
        AudioComponentInstanceDispose(impl_->audio_unit);
        impl_->audio_unit = nullptr;
        return false;
    }
    
    // Set buffer size
    UInt32 buffer_frames = (config_.buffer_size_ms * config_.sample_rate) / 1000;
    status = AudioUnitSetProperty(
        impl_->audio_unit,
        kAudioDevicePropertyBufferFrameSize,
        kAudioUnitScope_Global,
        0,
        &buffer_frames,
        sizeof(buffer_frames)
    );
    
    // Set callback
    AURenderCallbackStruct callback_struct = {};
    callback_struct.inputProc = CoreAudioCallback;
    callback_struct.inputProcRefCon = this;
    
    status = AudioUnitSetProperty(
        impl_->audio_unit,
        kAudioOutputUnitProperty_SetInputCallback,
        kAudioUnitScope_Global,
        0,
        &callback_struct,
        sizeof(callback_struct)
    );
    
    if (status != noErr) {
        std::cerr << "Failed to set audio callback: " << status << std::endl;
        AudioComponentInstanceDispose(impl_->audio_unit);
        impl_->audio_unit = nullptr;
        return false;
    }
    
    // Initialize audio unit
    status = AudioUnitInitialize(impl_->audio_unit);
    if (status != noErr) {
        std::cerr << "Failed to initialize audio unit: " << status << std::endl;
        AudioComponentInstanceDispose(impl_->audio_unit);
        impl_->audio_unit = nullptr;
        return false;
    }
    
    std::cout << "Core Audio initialized successfully" << std::endl;
    return true;
#else
    return false;
#endif
}

bool AudioCapture::initialize_miniaudio() {
    impl_->use_miniaudio = true;
    
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    // If we're forcing a single channel, capture all channels so we can select from them
    if (config_.force_single_channel) {
        config.capture.channels = 0; // 0 means use device's native channel count
    } else {
        config.capture.channels = config_.channels;
    }
    config.sampleRate = config_.sample_rate;
    config.dataCallback = MiniaudioCallback;
    config.pUserData = this;
    config.periodSizeInMilliseconds = config_.buffer_size_ms;
    
    // Find device by name if specified
    if (!config_.device_name.empty()) {
        ma_context context;
        if (ma_context_init(nullptr, 0, nullptr, &context) == MA_SUCCESS) {
            ma_device_info* capture_infos;
            ma_uint32 capture_count;
            
            if (ma_context_get_devices(&context, nullptr, nullptr, &capture_infos, &capture_count) == MA_SUCCESS) {
                for (ma_uint32 i = 0; i < capture_count; ++i) {
                    if (std::string(capture_infos[i].name).find(config_.device_name) != std::string::npos) {
                        config.capture.pDeviceID = &capture_infos[i].id;
                        break;
                    }
                }
            }
            
            ma_context_uninit(&context);
        }
    }
    
    if (ma_device_init(nullptr, &config, &impl_->ma_device) != MA_SUCCESS) {
        std::cerr << "Failed to initialize miniaudio device" << std::endl;
        return false;
    }
    
    // Store actual channel count
    impl_->actual_channels = impl_->ma_device.capture.channels;
    
    std::cout << "miniaudio initialized successfully" << std::endl;
    std::cout << "Device has " << impl_->actual_channels << " input channels" << std::endl;
    if (config_.force_single_channel) {
        std::cout << "Will use channel " << (config_.input_channel_index + 1) 
                 << " (Input " << (config_.input_channel_index + 1) << ")" << std::endl;
    }
    return true;
}

void AudioCapture::shutdown() {
    stop();
    
#ifdef __APPLE__
    if (impl_->audio_unit) {
        AudioUnitUninitialize(impl_->audio_unit);
        AudioComponentInstanceDispose(impl_->audio_unit);
        impl_->audio_unit = nullptr;
    }
#endif
    
    if (impl_->use_miniaudio) {
        ma_device_uninit(&impl_->ma_device);
    }
}

bool AudioCapture::start() {
    if (running_.load()) return true;
    
#ifdef __APPLE__
    if (impl_->audio_unit) {
        OSStatus status = AudioOutputUnitStart(impl_->audio_unit);
        if (status != noErr) {
            std::cerr << "Failed to start audio unit: " << status << std::endl;
            return false;
        }
        running_ = true;
        return true;
    }
#endif
    
    if (impl_->use_miniaudio) {
        if (ma_device_start(&impl_->ma_device) != MA_SUCCESS) {
            std::cerr << "Failed to start miniaudio device" << std::endl;
            return false;
        }
        running_ = true;
        return true;
    }
    
    return false;
}

void AudioCapture::stop() {
    if (!running_.load()) return;
    
    running_ = false;
    
#ifdef __APPLE__
    if (impl_->audio_unit) {
        AudioOutputUnitStop(impl_->audio_unit);
    }
#endif
    
    if (impl_->use_miniaudio) {
        ma_device_stop(&impl_->ma_device);
    }
}

void AudioCapture::process_audio_callback(const float* input, size_t frame_count) {
    if (callback_ && running_.load()) {
        callback_(input, frame_count);
    }
}

std::vector<DeviceInfo> AudioCapture::enumerate_devices() {
    std::vector<DeviceInfo> devices;
    
#ifdef __APPLE__
    // Get all audio devices
    AudioObjectPropertyAddress addr = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };
    
    UInt32 size;
    AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, nullptr, &size);
    
    std::vector<AudioDeviceID> device_ids(size / sizeof(AudioDeviceID));
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, nullptr, &size, device_ids.data());
    
    for (AudioDeviceID device_id : device_ids) {
        DeviceInfo info;
        
        // Get device name
        CFStringRef name_ref;
        UInt32 name_size = sizeof(name_ref);
        AudioObjectPropertyAddress name_addr = {
            kAudioDevicePropertyDeviceNameCFString,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        
        if (AudioObjectGetPropertyData(device_id, &name_addr, 0, nullptr, 
                                     &name_size, &name_ref) == noErr) {
            char name_buf[256];
            CFStringGetCString(name_ref, name_buf, sizeof(name_buf), kCFStringEncodingUTF8);
            CFRelease(name_ref);
            info.name = name_buf;
        }
        
        // Get UID
        CFStringRef uid_ref;
        UInt32 uid_size = sizeof(uid_ref);
        AudioObjectPropertyAddress uid_addr = {
            kAudioDevicePropertyDeviceUID,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        
        if (AudioObjectGetPropertyData(device_id, &uid_addr, 0, nullptr, 
                                     &uid_size, &uid_ref) == noErr) {
            char uid_buf[256];
            CFStringGetCString(uid_ref, uid_buf, sizeof(uid_buf), kCFStringEncodingUTF8);
            CFRelease(uid_ref);
            info.id = uid_buf;
        }
        
        // Check input channels
        AudioObjectPropertyAddress input_addr = {
            kAudioDevicePropertyStreamConfiguration,
            kAudioDevicePropertyScopeInput,
            kAudioObjectPropertyElementMain
        };
        
        AudioObjectGetPropertyDataSize(device_id, &input_addr, 0, nullptr, &size);
        if (size > sizeof(AudioBufferList)) {
            info.max_input_channels = 1; // Simplified
        }
        
        // Check if default
        AudioDeviceID default_input;
        UInt32 default_size = sizeof(default_input);
        AudioObjectPropertyAddress default_addr = {
            kAudioHardwarePropertyDefaultInputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain
        };
        
        if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &default_addr, 0, nullptr, 
                                     &default_size, &default_input) == noErr) {
            info.is_default_input = (device_id == default_input);
        }
        
        if (info.max_input_channels > 0) {
            devices.push_back(info);
        }
    }
#else
    // Use miniaudio for enumeration
    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) == MA_SUCCESS) {
        ma_device_info* capture_infos;
        ma_uint32 capture_count;
        
        if (ma_context_get_devices(&context, nullptr, nullptr, &capture_infos, &capture_count) == MA_SUCCESS) {
            for (ma_uint32 i = 0; i < capture_count; ++i) {
                DeviceInfo info;
                info.name = capture_infos[i].name;
                info.max_input_channels = capture_infos[i].nativeDataFormats[0].channels;
                info.default_sample_rate = capture_infos[i].nativeDataFormats[0].sampleRate;
                info.is_default_input = capture_infos[i].isDefault;
                devices.push_back(info);
            }
        }
        
        ma_context_uninit(&context);
    }
#endif
    
    return devices;
}

DeviceInfo AudioCapture::get_current_device() const {
    DeviceInfo info;
    info.name = config_.device_name;
    info.max_input_channels = config_.channels;
    info.default_sample_rate = config_.sample_rate;
    return info;
}

size_t AudioCapture::read_samples(float* buffer, size_t max_samples) {
    // Not implemented for callback-based capture
    return 0;
}

} // namespace audio
} // namespace rt_stt
