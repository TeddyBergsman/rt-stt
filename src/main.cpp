#include "stt/engine.h"
#include "stt/whisper_wrapper.h"
#include "audio/capture.h"
#include "ipc/server.h"
#include "config/config.h"
#include <iostream>
#include <fstream>
#include <signal.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>
#include <sys/stat.h>

// Global flag for graceful shutdown
std::atomic<bool> g_shutdown_requested(false);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\nShutdown requested..." << std::endl;
        g_shutdown_requested = true;
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "RT-STT Daemon v1.0" << std::endl;
    std::cout << "==================" << std::endl;
    
    // Parse command line arguments
    std::string config_file;
    std::string socket_path = "/tmp/rt-stt.sock";
    
    // Default config path
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : "";
    if (!home.empty()) {
        config_file = home + "/Library/Application Support/rt-stt/config.json";
    }
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if ((arg == "-c" || arg == "--config") && i + 1 < argc) {
            config_file = argv[++i];
        } else if ((arg == "-s" || arg == "--socket") && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  -c, --config <file>   Configuration file (default: /etc/rt-stt/config.json)" << std::endl;
            std::cout << "  -s, --socket <path>   Unix socket path (default: /tmp/rt-stt.sock)" << std::endl;
            std::cout << "  -h, --help           Show this help message" << std::endl;
            return 0;
        }
    }
    
    // Initialize STT engine
    rt_stt::stt::STTEngine stt_engine;
    rt_stt::stt::STTEngine::Config stt_config;
    rt_stt::audio::CaptureConfig capture_config;
    
    // Load configuration from file if it exists
    bool config_loaded = false;
    struct stat buffer;
    if (!config_file.empty() && stat(config_file.c_str(), &buffer) == 0) {
        try {
            std::ifstream config_stream(config_file);
            nlohmann::json config_json;
            config_stream >> config_json;
            
            // Parse STT configuration
            if (config_json.contains("stt")) {
                auto& stt = config_json["stt"];
                if (stt.contains("model")) {
                    auto& model = stt["model"];
                    stt_config.model_config.model_path = model.value("path", "models/ggml-small.en.bin");
                    stt_config.model_config.language = model.value("language", "en");
                    stt_config.model_config.use_gpu = model.value("use_gpu", true);
                    stt_config.model_config.n_threads = model.value("n_threads", 4);
                    stt_config.model_config.beam_size = model.value("beam_size", 5);
                    stt_config.model_config.temperature = model.value("temperature", 0.0f);
                }
                if (stt.contains("vad")) {
                    auto& vad = stt["vad"];
                    stt_config.vad_config.energy_threshold = vad.value("energy_threshold", 0.001f);
                    stt_config.vad_config.speech_start_ms = vad.value("speech_start_ms", 150);
                    stt_config.vad_config.speech_end_ms = vad.value("speech_end_ms", 1000);
                    stt_config.vad_config.min_speech_ms = vad.value("min_speech_ms", 500);
                    stt_config.vad_config.speech_start_threshold = vad.value("speech_start_threshold", 1.08f);
                    stt_config.vad_config.speech_end_threshold = vad.value("speech_end_threshold", 0.85f);
                    stt_config.vad_config.pre_speech_buffer_ms = vad.value("pre_speech_buffer_ms", 500);
                    stt_config.vad_config.noise_floor_adaptation_rate = vad.value("noise_floor_adaptation_rate", 0.01f);
                    stt_config.vad_config.use_adaptive_threshold = vad.value("use_adaptive_threshold", true);
                }
                if (stt.contains("audio")) {
                    auto& audio = stt["audio"];
                    capture_config.device_name = audio.value("device_name", "MOTU M2");
                    capture_config.sample_rate = audio.value("sample_rate", 16000);
                    capture_config.channels = audio.value("channels", 1);
                    capture_config.buffer_size_ms = audio.value("buffer_size_ms", 30);
                    capture_config.input_channel_index = audio.value("input_channel_index", 1);
                    capture_config.force_single_channel = audio.value("force_single_channel", true);
                    capture_config.use_callback = true;
                }
            }
            
            // Parse IPC configuration
            if (config_json.contains("ipc")) {
                auto& ipc = config_json["ipc"];
                socket_path = ipc.value("socket_path", socket_path);
            }
            
            config_loaded = true;
            std::cout << "Loaded configuration from: " << config_file << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to load configuration: " << e.what() << std::endl;
            std::cerr << "Using default configuration" << std::endl;
        }
    }
    
    // Use default configuration if not loaded from file
    if (!config_loaded) {
        // Configure STT with defaults
        stt_config.model_config.model_path = "models/ggml-small.en.bin";
        stt_config.model_config.language = "en";
        stt_config.model_config.use_gpu = true;
        
        // Configure VAD with tuned settings
        stt_config.vad_config.energy_threshold = 0.001f;
        stt_config.vad_config.speech_start_ms = 150;
        stt_config.vad_config.speech_end_ms = 1000;
        stt_config.vad_config.min_speech_ms = 500;
        stt_config.vad_config.speech_start_threshold = 1.08f;
        stt_config.vad_config.speech_end_threshold = 0.85f;
        stt_config.vad_config.pre_speech_buffer_ms = 500;
        stt_config.vad_config.noise_floor_adaptation_rate = 0.01f;
        stt_config.vad_config.use_adaptive_threshold = true;
        
        // Configure audio capture
        capture_config.device_name = "MOTU M2";
        capture_config.sample_rate = 16000;
        capture_config.channels = 1;
        capture_config.buffer_size_ms = 30;
        capture_config.use_callback = true;
        capture_config.input_channel_index = 1;
        capture_config.force_single_channel = true;
    }
    
    // Create audio capture instance
    rt_stt::audio::AudioCapture audio_capture;
    
    std::cout << "Initializing audio capture..." << std::endl;
    if (!audio_capture.initialize(capture_config)) {
        std::cerr << "Failed to initialize audio capture" << std::endl;
        return 1;
    }
    
    std::cout << "Initializing STT engine..." << std::endl;
    if (!stt_engine.initialize(stt_config)) {
        std::cerr << "Failed to initialize STT engine" << std::endl;
        return 1;
    }
    
    // Keep config path for saving
    std::string config_path = config_file;
    
    // Keep config as mutable JSON for runtime updates
    nlohmann::json current_config;
    current_config["model_config"] = {
        {"model_path", stt_config.model_config.model_path},
        {"language", stt_config.model_config.language},
        {"n_threads", stt_config.model_config.n_threads},
        {"use_gpu", stt_config.model_config.use_gpu},
        {"beam_size", stt_config.model_config.beam_size},
        {"temperature", stt_config.model_config.temperature}
    };
    current_config["vad_config"] = {
        {"use_adaptive_threshold", stt_config.vad_config.use_adaptive_threshold},
        {"energy_threshold", stt_config.vad_config.energy_threshold},
        {"speech_start_ms", stt_config.vad_config.speech_start_ms},
        {"speech_end_ms", stt_config.vad_config.speech_end_ms},
        {"min_speech_ms", stt_config.vad_config.min_speech_ms},
        {"pre_speech_buffer_ms", stt_config.vad_config.pre_speech_buffer_ms},
        {"noise_floor_adaptation_rate", stt_config.vad_config.noise_floor_adaptation_rate},
        {"speech_start_threshold", stt_config.vad_config.speech_start_threshold},
        {"speech_end_threshold", stt_config.vad_config.speech_end_threshold}
    };
    current_config["audio_capture_config"] = {
        {"device_name", capture_config.device_name},
        {"sample_rate", capture_config.sample_rate},
        {"channels", capture_config.channels},
        {"buffer_size_ms", capture_config.buffer_size_ms},
        {"force_single_channel", capture_config.force_single_channel},
        {"input_channel_index", capture_config.input_channel_index}
    };
    current_config["ipc_socket_path"] = socket_path;
    
    // Initialize IPC server
    rt_stt::ipc::Server ipc_server;
    
    std::cout << "Initializing IPC server on " << socket_path << "..." << std::endl;
    if (!ipc_server.initialize(socket_path)) {
        std::cerr << "Failed to initialize IPC server" << std::endl;
        return 1;
    }
    
    // Set up transcription callback
    stt_engine.set_transcription_callback(
        [&ipc_server](const rt_stt::stt::TranscriptionResult& result) {
            if (!result.text.empty()) {
                std::cout << "[TRANSCRIPTION] " << result.text << std::endl;
                std::cout << "[DEBUG] Broadcasting to IPC clients..." << std::endl;
                
                // Create full JSON with all metadata
                nlohmann::json transcription_data;
                transcription_data["text"] = result.text;
                transcription_data["confidence"] = result.confidence;
                transcription_data["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
                transcription_data["language"] = result.language;
                transcription_data["language_probability"] = result.language_probability;
                transcription_data["processing_time_ms"] = result.processing_time.count();
                transcription_data["audio_duration_ms"] = result.audio_duration_ms;
                transcription_data["model"] = result.model_name;
                transcription_data["is_final"] = result.is_final;
                
                // Add segments with full metadata
                transcription_data["segments"] = nlohmann::json::array();
                for (const auto& segment : result.segments) {
                    nlohmann::json seg;
                    seg["id"] = segment.id;
                    seg["seek"] = segment.seek;
                    seg["start"] = segment.start;
                    seg["end"] = segment.end;
                    seg["text"] = segment.text;
                    seg["tokens"] = segment.tokens;
                    seg["temperature"] = segment.temperature;
                    seg["avg_logprob"] = segment.avg_logprob;
                    seg["compression_ratio"] = segment.compression_ratio;
                    seg["no_speech_prob"] = segment.no_speech_prob;
                    transcription_data["segments"].push_back(seg);
                }
                
                ipc_server.broadcast_transcription_full(transcription_data);
                std::cout << "[DEBUG] Broadcast complete. Connected clients: " << ipc_server.get_client_count() << std::endl;
            }
        }
    );
    
    // Set up command handler
    ipc_server.set_command_handler(
        [&stt_engine, &stt_config, &capture_config, &current_config, config_path, &ipc_server](const std::string& action, const nlohmann::json& params) -> nlohmann::json {
            nlohmann::json result;
            
            try {
                if (action == "pause") {
                    stt_engine.pause();
                    result["status"] = "paused";
                    result["listening"] = false;
                } else if (action == "resume") {
                    stt_engine.resume();
                    result["status"] = "listening";
                    result["listening"] = true;
                } else if (action == "get_status") {
                    result["listening"] = stt_engine.is_running();
                    result["model"] = stt_config.model_config.model_path;
                    result["language"] = stt_config.model_config.language;
                    result["vad_enabled"] = stt_config.vad_config.use_adaptive_threshold;
                    result["clients"] = ipc_server.get_client_count();
                } else if (action == "get_config") {
                    // Return full configuration
                    result = current_config;
                } else if (action == "set_config") {
                    // Update configuration with validation
                    auto new_config = params.value("config", nlohmann::json::object());
                    
                    // Merge with existing config
                    current_config.merge_patch(new_config);
                    
                    // Apply changes
                    bool needs_restart = false;
                    
                    // VAD settings
                    if (new_config.contains("vad_config")) {
                        stt_config.vad_config.use_adaptive_threshold = current_config["vad_config"]["use_adaptive_threshold"].get<bool>();
                        stt_config.vad_config.energy_threshold = current_config["vad_config"]["energy_threshold"].get<float>();
                        stt_config.vad_config.speech_start_ms = current_config["vad_config"]["speech_start_ms"].get<int>();
                        stt_config.vad_config.speech_end_ms = current_config["vad_config"]["speech_end_ms"].get<int>();
                        stt_config.vad_config.min_speech_ms = current_config["vad_config"]["min_speech_ms"].get<int>();
                        stt_config.vad_config.pre_speech_buffer_ms = current_config["vad_config"]["pre_speech_buffer_ms"].get<int>();
                        stt_config.vad_config.noise_floor_adaptation_rate = current_config["vad_config"]["noise_floor_adaptation_rate"].get<float>();
                        stt_config.vad_config.speech_start_threshold = current_config["vad_config"]["speech_start_threshold"].get<float>();
                        stt_config.vad_config.speech_end_threshold = current_config["vad_config"]["speech_end_threshold"].get<float>();
                        stt_config.vad_config.use_adaptive_threshold = current_config["vad_config"]["use_adaptive_threshold"].get<bool>();
                        
                        stt_engine.update_vad_config(stt_config.vad_config);
                        result["vad_updated"] = true;
                    }
                    
                    // Model settings
                    if (new_config.contains("model_config")) {
                        if (new_config["model_config"].contains("model_path")) {
                            std::string new_model = current_config["model_config"]["model_path"].get<std::string>();
                            stt_engine.set_model(new_model);
                            stt_config.model_config.model_path = new_model;
                            result["model_updated"] = true;
                        }
                        
                        if (new_config["model_config"].contains("language")) {
                            std::string new_lang = current_config["model_config"]["language"].get<std::string>();
                            stt_engine.set_language(new_lang);
                            stt_config.model_config.language = new_lang;
                            result["language_updated"] = true;
                        }
                    }
                    
                    // Save config to file if requested
                    if (params.value("save", true)) {
                        std::ofstream config_file(config_path);
                        if (config_file) {
                            config_file << current_config.dump(4);
                            config_file.close();
                            result["config_saved"] = true;
                        }
                    }
                    
                    result["success"] = true;
                } else if (action == "set_language") {
                    std::string lang = params.value("language", "en");
                    stt_engine.set_language(lang);
                    stt_config.model_config.language = lang;
                        current_config["model_config"]["language"] = lang;
                    result["language"] = lang;
                } else if (action == "set_model") {
                    std::string model = params.value("model", "");
                    if (!model.empty()) {
                        stt_engine.set_model(model);
                        stt_config.model_config.model_path = model;
                        current_config["model_config"]["model_path"] = model;
                        result["model"] = model;
                    }
                } else if (action == "set_vad_sensitivity") {
                    float sensitivity = params.value("sensitivity", 1.08f);
                    stt_config.vad_config.speech_start_threshold = sensitivity;
                        current_config["vad_config"]["speech_start_threshold"] = sensitivity;
                    stt_engine.update_vad_config(stt_config.vad_config);
                    result["sensitivity"] = sensitivity;
                } else if (action == "get_metrics") {
                    auto metrics = stt_engine.get_metrics();
                    result["avg_latency_ms"] = metrics.avg_latency_ms;
                    result["avg_rtf"] = metrics.avg_rtf;
                    result["cpu_usage"] = metrics.cpu_usage;
                    result["memory_usage_mb"] = metrics.memory_usage_mb;
                    result["transcriptions_count"] = metrics.transcriptions_count;
                } else {
                    throw std::runtime_error("Unknown action: " + action);
                }
            } catch (const std::exception& e) {
                result["error"] = e.what();
                result["success"] = false;
            }
            
            return result;
        }
    );
    
    // Start IPC server
    if (!ipc_server.start()) {
        std::cerr << "Failed to start IPC server" << std::endl;
        return 1;
    }
    
    // Start STT engine
    // Set up audio callback to feed the STT engine
    audio_capture.set_callback(
        [&stt_engine](const float* samples, size_t n_samples) {
            stt_engine.feed_audio(samples, n_samples);
        }
    );
    
    // Start audio capture
    std::cout << "Starting audio capture..." << std::endl;
    if (!audio_capture.start()) {
        std::cerr << "Failed to start audio capture" << std::endl;
        return 1;
    }
    
    // Start STT engine
    std::cout << "Starting STT engine..." << std::endl;
    stt_engine.start();
    
    if (!stt_engine.is_running()) {
        std::cerr << "Failed to start STT engine" << std::endl;
        return 1;
    }
    
    std::cout << "RT-STT daemon is running" << std::endl;
    std::cout << "Listening on: " << socket_path << std::endl;
    std::cout << "Audio device: " << capture_config.device_name << " (Input " << (capture_config.input_channel_index + 1) << ")" << std::endl;
    std::cout << "Model: " << stt_config.model_config.model_path << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl;
    std::cout << std::endl;
    std::cout << "Waiting for client connections..." << std::endl;
    
    // Main loop
    while (!g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Periodically broadcast status
        static auto last_status_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_status_time).count() >= 30) {
            nlohmann::json status = {
                {"listening", stt_engine.is_running()},
                {"clients", ipc_server.get_client_count()},
                {"uptime", std::chrono::duration_cast<std::chrono::seconds>(
                    now - last_status_time).count()}
            };
            ipc_server.broadcast_status(status);
            last_status_time = now;
        }
    }
    
    std::cout << "Shutting down..." << std::endl;
    
    // Stop services
    audio_capture.stop();
    stt_engine.stop();
    ipc_server.stop();
    
    // Cleanup
    audio_capture.shutdown();
    stt_engine.shutdown();
    ipc_server.shutdown();
    
    std::cout << "RT-STT daemon stopped" << std::endl;
    return 0;
}