#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <nlohmann/json.hpp>

namespace rt_stt {
namespace ipc {

// Message types for IPC protocol
struct Message {
    enum Type {
        // Client -> Server
        COMMAND,
        SUBSCRIBE,
        UNSUBSCRIBE,
        
        // Server -> Client
        TRANSCRIPTION,
        STATUS,
        ERROR,
        ACKNOWLEDGMENT
    };
    
    Type type;
    std::string id;  // Unique message ID
    nlohmann::json data;
};

// Client connection info
struct ClientInfo {
    int socket_fd;
    std::string id;
    bool subscribed_to_transcriptions;
    std::thread handler_thread;
};

class Server {
public:
    using TranscriptionCallback = std::function<void(const std::string&, float confidence)>;
    using CommandHandler = std::function<nlohmann::json(const std::string& action, const nlohmann::json& params)>;
    
    Server();
    ~Server();
    
    // Initialize server with socket path
    bool initialize(const std::string& socket_path);
    
    // Start/stop server
    bool start();
    void stop();
    void shutdown();
    
    // Register handlers
    void set_command_handler(CommandHandler handler) { command_handler_ = handler; }
    
    // Broadcast transcription to all subscribed clients
    void broadcast_transcription(const std::string& text, float confidence = 1.0f);
    
    // Send status update to all clients
    void broadcast_status(const nlohmann::json& status);
    
    // Get number of connected clients
    size_t get_client_count() const;
    
    // Check if server is running
    bool is_running() const { return running_.load(); }
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_requested_{false};
    
    CommandHandler command_handler_;
    
    // Server thread functions
    void accept_connections();
    void handle_client(int client_fd);
    
    // Message handling
    bool send_message(int client_fd, const Message& msg);
    bool receive_message(int client_fd, Message& msg);
    void process_message(int client_fd, const Message& msg);
    
    // Cleanup
    void cleanup_client(int client_fd);
};

} // namespace ipc
} // namespace rt_stt

#endif // IPC_SERVER_H
