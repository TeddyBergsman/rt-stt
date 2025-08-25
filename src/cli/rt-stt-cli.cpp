#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <signal.h>
#include <arpa/inet.h>

// Global flag for graceful shutdown
std::atomic<bool> g_shutdown_requested(false);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_shutdown_requested = true;
    }
}

class RTSTTClient {
public:
    RTSTTClient(const std::string& socket_path = "/tmp/rt-stt.sock")
        : socket_path_(socket_path), socket_fd_(-1) {}
    
    ~RTSTTClient() {
        disconnect();
    }
    
    bool connect() {
        socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_path_.c_str(), sizeof(addr.sun_path) - 1);
        
        if (::connect(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Failed to connect to " << socket_path_ << std::endl;
            std::cerr << "Is the RT-STT daemon running?" << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
        
#ifdef __APPLE__
        int nosigpipe = 1;
        setsockopt(socket_fd_, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
#endif
        
        return true;
    }
    
    void disconnect() {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
    }
    
    bool send_command(const std::string& action, const nlohmann::json& params = {}) {
        if (socket_fd_ < 0) return false;
        
        nlohmann::json msg = {
            {"type", 0},  // COMMAND
            {"id", std::to_string(std::chrono::system_clock::now().time_since_epoch().count())},
            {"data", {
                {"action", action},
                {"params", params}
            }}
        };
        
        return send_message(msg);
    }
    
    bool stream_transcriptions(bool json_output = false, bool timestamps = false) {
        if (socket_fd_ < 0) return false;
        
        // Subscribe to transcriptions
        nlohmann::json msg = {
            {"type", 1},  // SUBSCRIBE
            {"id", std::to_string(std::chrono::system_clock::now().time_since_epoch().count())},
            {"data", {}}
        };
        
        if (!send_message(msg)) {
            return false;
        }
        
        // Receive messages
        while (!g_shutdown_requested.load()) {
            nlohmann::json response;
            if (!receive_message(response)) {
                break;
            }
            
            int msg_type = response["type"];
            auto& data = response["data"];
            
            if (msg_type == 3) {  // TRANSCRIPTION
                if (json_output) {
                    std::cout << response.dump() << std::endl;
                } else {
                    if (timestamps) {
                        auto now = std::chrono::system_clock::now();
                        auto time_t = std::chrono::system_clock::to_time_t(now);
                        std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "] ";
                    }
                    std::cout << data["text"].get<std::string>() << std::endl;
                }
            }
        }
        
        return true;
    }
    
    bool get_status() {
        if (!send_command("get_status")) {
            return false;
        }
        
        nlohmann::json response;
        if (!receive_message(response)) {
            return false;
        }
        
        if (response["type"] == 6 && response["data"]["success"]) {  // ACKNOWLEDGMENT
            auto& result = response["data"]["result"];
            std::cout << "RT-STT Status:" << std::endl;
            std::cout << "  Listening: " << (result["listening"] ? "Yes" : "No") << std::endl;
            std::cout << "  Model: " << result["model"] << std::endl;
            std::cout << "  Language: " << result["language"] << std::endl;
            std::cout << "  VAD Enabled: " << (result["vad_enabled"] ? "Yes" : "No") << std::endl;
        }
        
        return true;
    }
    
    bool get_config(bool json_output) {
        if (!send_command("get_config")) {
            return false;
        }
        
        nlohmann::json response;
        if (!receive_message(response)) {
            return false;
        }
        
        if (response["type"] == 6 && response["data"]["success"]) {  // ACKNOWLEDGMENT
            auto& result = response["data"]["result"];
            if (json_output) {
                std::cout << result.dump(2) << std::endl;
            } else {
                std::cout << "RT-STT Configuration:" << std::endl;
                std::cout << result.dump(2) << std::endl;
            }
        }
        
        return true;
    }
    
    bool get_metrics(bool json_output) {
        if (!send_command("get_metrics")) {
            return false;
        }
        
        nlohmann::json response;
        if (!receive_message(response)) {
            return false;
        }
        
        if (response["type"] == 6 && response["data"]["success"]) {  // ACKNOWLEDGMENT
            auto& result = response["data"]["result"];
            if (json_output) {
                std::cout << result.dump(2) << std::endl;
            } else {
                std::cout << "RT-STT Performance Metrics:" << std::endl;
                std::cout << "  Average Latency: " << result["avg_latency_ms"].get<float>() << " ms" << std::endl;
                std::cout << "  Average RTF: " << result["avg_rtf"].get<float>() << std::endl;
                std::cout << "  CPU Usage: " << result["cpu_usage"].get<float>() << "%" << std::endl;
                std::cout << "  Memory Usage: " << result["memory_usage_mb"].get<size_t>() << " MB" << std::endl;
                std::cout << "  Transcriptions: " << result["transcriptions_count"].get<size_t>() << std::endl;
            }
        }
        
        return true;
    }
    
private:
    std::string socket_path_;
    int socket_fd_;
    
    bool send_message(const nlohmann::json& msg) {
        std::string serialized = msg.dump();
        uint32_t length = htonl(serialized.length());
        
        if (send(socket_fd_, &length, sizeof(length), 0) != sizeof(length)) {
            return false;
        }
        
        if (send(socket_fd_, serialized.c_str(), serialized.length(), 0) != serialized.length()) {
            return false;
        }
        
        return true;
    }
    
    bool receive_message(nlohmann::json& msg) {
        uint32_t length_prefix;
        if (recv(socket_fd_, &length_prefix, sizeof(length_prefix), MSG_WAITALL) != sizeof(length_prefix)) {
            return false;
        }
        
        uint32_t length = ntohl(length_prefix);
        if (length > 1024 * 1024) {  // Max 1MB
            return false;
        }
        
        std::vector<char> buffer(length);
        if (recv(socket_fd_, buffer.data(), length, MSG_WAITALL) != length) {
            return false;
        }
        
        try {
            msg = nlohmann::json::parse(std::string(buffer.data(), length));
            return true;
        } catch (...) {
            return false;
        }
    }
};

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [command] [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  stream          Stream transcriptions (default)" << std::endl;
    std::cout << "  status          Get daemon status" << std::endl;
    std::cout << "  pause           Pause listening" << std::endl;
    std::cout << "  resume          Resume listening" << std::endl;
    std::cout << "  set-language    Set recognition language" << std::endl;
    std::cout << "  get-config      Get current configuration" << std::endl;
    std::cout << "  get-metrics     Get performance metrics" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -s, --socket    Socket path (default: /tmp/rt-stt.sock)" << std::endl;
    std::cout << "  -j, --json      Output in JSON format" << std::endl;
    std::cout << "  -t, --timestamp Add timestamps to output" << std::endl;
    std::cout << "  -h, --help      Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << program << " stream               # Stream transcriptions" << std::endl;
    std::cout << "  " << program << " stream -j            # Stream as JSON" << std::endl;
    std::cout << "  " << program << " stream -t            # Stream with timestamps" << std::endl;
    std::cout << "  " << program << " status               # Check daemon status" << std::endl;
    std::cout << "  " << program << " pause                # Pause listening" << std::endl;
    std::cout << "  " << program << " set-language es      # Set Spanish" << std::endl;
    std::cout << "  " << program << " get-config -j        # Get config as JSON" << std::endl;
}

int main(int argc, char* argv[]) {
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    // Parse arguments
    std::string command = "stream";
    std::string socket_path = "/tmp/rt-stt.sock";
    bool json_output = false;
    bool timestamps = false;
    std::vector<std::string> args;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-s" || arg == "--socket") {
            if (i + 1 < argc) {
                socket_path = argv[++i];
            }
        } else if (arg == "-j" || arg == "--json") {
            json_output = true;
        } else if (arg == "-t" || arg == "--timestamp") {
            timestamps = true;
        } else if (arg[0] != '-') {
            if (command == "stream") {
                command = arg;
            } else {
                args.push_back(arg);
            }
        }
    }
    
    // Create client
    RTSTTClient client(socket_path);
    
    if (!client.connect()) {
        return 1;
    }
    
    // Execute command
    bool success = false;
    
    if (command == "stream") {
        success = client.stream_transcriptions(json_output, timestamps);
    } else if (command == "status") {
        success = client.get_status();
    } else if (command == "pause") {
        success = client.send_command("pause");
        if (success) std::cout << "Listening paused" << std::endl;
    } else if (command == "resume") {
        success = client.send_command("resume");
        if (success) std::cout << "Listening resumed" << std::endl;
    } else if (command == "set-language" && !args.empty()) {
        nlohmann::json params = {{"language", args[0]}};
        success = client.send_command("set_language", params);
        if (success) std::cout << "Language set to: " << args[0] << std::endl;
    } else if (command == "get-config") {
        success = client.get_config(json_output);
    } else if (command == "get-metrics") {
        success = client.get_metrics(json_output);
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    return success ? 0 : 1;
}
