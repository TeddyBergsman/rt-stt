#include "stt/engine.h"
#include "audio/capture.h"
#include "utils/terminal_output.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>

std::atomic<bool> g_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\n\nShutting down..." << std::endl;
        g_running = false;
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handler
    std::signal(SIGINT, signal_handler);
    
    std::cout << "RT-STT Test Application" << std::endl;
    std::cout << "======================" << std::endl;
    std::cout << "Press Ctrl+C to exit\n" << std::endl;
    
    // Configure STT engine
    rt_stt::stt::STTEngine::Config config;
    
    // Model configuration
    config.model_config.model_path = "models/ggml-base.en.bin";
    config.model_config.language = "en";
    config.model_config.n_threads = 4;
    config.model_config.use_gpu = true;
    config.model_config.beam_size = 5;
    
    // Check for command line arguments (debug output disabled)
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--model" && i + 1 < argc) {
            config.model_config.model_path = argv[++i];
        } else if (arg == "--language" && i + 1 < argc) {
            config.model_config.language = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            config.model_config.n_threads = std::stoi(argv[++i]);
        } else if (arg == "--no-gpu") {
            config.model_config.use_gpu = false;
        } else if (arg == "--translate") {
            config.model_config.translate = true;
        } else if (arg == "--no-vad") {
            // Disable VAD for testing - process all audio
            config.vad_config.energy_threshold = 0.0f;
            config.vad_config.use_adaptive_threshold = false;
            std::cout << "VAD disabled - processing all audio\n";
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --model PATH       Path to Whisper model (default: models/ggml-base.en.bin)\n";
            std::cout << "  --language LANG    Language code (default: en, use 'auto' for detection)\n";
            std::cout << "  --threads N        Number of threads (default: 4)\n";
            std::cout << "  --no-gpu           Disable GPU acceleration\n";
            std::cout << "  --translate        Translate to English\n";
            std::cout << "  --device NAME      Audio device name (default: MOTU M2)\n";
            std::cout << "  --no-vad           Disable VAD (process all audio)\n";
            return 0;
        }
    }
    
    // VAD configuration - tuned for normal speaking voice
    // When adaptive threshold is true, these are MULTIPLIERS of noise floor
    config.vad_config.energy_threshold = 0.001f;     // Base threshold (for non-adaptive mode)
    config.vad_config.speech_start_threshold = 1.08f; // Speech must be 1.08x noise floor
    config.vad_config.speech_end_threshold = 0.85f;   // End when below 0.85x noise floor (more hysteresis)
    config.vad_config.speech_start_ms = 150;         // 150ms to confirm speech start
    config.vad_config.speech_end_ms = 1000;          // 1 second silence to end speech
    config.vad_config.use_adaptive_threshold = true; // Use noise floor adaptation
    config.vad_config.min_speech_ms = 300;           // Min 300ms speech to process
    config.vad_config.pre_speech_buffer_ms = 500;    // Keep 500ms before speech detected
    config.vad_config.noise_floor_adaptation_rate = 0.01f; // Faster adaptation to noise
    
    // Enable terminal output for validation
    config.enable_terminal_output = true;
    config.measure_performance = true;
    
    // Create and initialize STT engine
    rt_stt::stt::STTEngine engine;
    
    std::cout << "Loading model: " << config.model_config.model_path << std::endl;
    
    // VAD config set
    
    if (!engine.initialize(config)) {
        std::cerr << "Failed to initialize STT engine" << std::endl;
        return 1;
    }
    
    // Set up transcription callback
    engine.set_transcription_callback([](const rt_stt::stt::TranscriptionResult& result) {
        // Additional processing can be done here
        // The terminal output is already handled by the engine
    });
    
    // Configure audio capture
    rt_stt::audio::CaptureConfig audio_config;
    audio_config.device_name = "MOTU M2";
    audio_config.sample_rate = 16000;
    audio_config.channels = 1;
    audio_config.buffer_size_ms = 30;
    
    // Check for audio device override
    for (int i = 1; i < argc - 1; i++) {
        if (std::string(argv[i]) == "--device") {
            audio_config.device_name = argv[i + 1];
            break;
        }
    }
    
    // Create audio capture
    rt_stt::audio::AudioCapture capture;
    
    std::cout << "\nInitializing audio capture..." << std::endl;
    std::cout << "Looking for device: " << audio_config.device_name << std::endl;
    
    if (!capture.initialize(audio_config)) {
        // Try default device
        std::cerr << "Failed to initialize audio device: " << audio_config.device_name << std::endl;
        std::cerr << "Trying default audio device..." << std::endl;
        
        audio_config.device_name = "";
        if (!capture.initialize(audio_config)) {
            std::cerr << "Failed to initialize default audio device" << std::endl;
            return 1;
        }
    }
    
    // Set audio callback
    capture.set_callback([&engine](const float* samples, size_t n_samples) {
        engine.feed_audio(samples, n_samples);
    });
    
    // Start engine and capture
    engine.start();
    
    std::cout << "\nStarting audio capture..." << std::endl;
    if (!capture.start()) {
        std::cerr << "Failed to start audio capture" << std::endl;
        return 1;
    }
    
    std::cout << "\nListening... Speak into your microphone." << std::endl;
    std::cout << "Real-time transcription will appear below." << std::endl;
    std::cout << "Press Ctrl+C to stop.\n" << std::endl;
    
    // Run until interrupted
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Check for keyboard input for interactive commands
        // (Would need platform-specific implementation for non-blocking input)
    }
    
    // Cleanup
    std::cout << "\nStopping audio capture..." << std::endl;
    capture.stop();
    
    std::cout << "Stopping STT engine..." << std::endl;
    engine.stop();
    
    // Print final metrics
    auto metrics = engine.get_metrics();
    std::cout << "\n=== Final Metrics ===";
    std::cout << "\nTranscriptions: " << metrics.transcriptions_count;
    std::cout << "\nAverage latency: " << metrics.avg_latency_ms << " ms";
    std::cout << "\nAverage RTF: " << metrics.avg_rtf;
    std::cout << "\nProcessed samples: " << metrics.processed_samples;
    std::cout << "\nProcessed time: " << (metrics.processed_samples / 16000.0) << " seconds";
    std::cout << std::endl;
    
    return 0;
}
