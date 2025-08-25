#include "ipc/server.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <map>
#include <sstream>
#include <chrono>
#include <arpa/inet.h>

// macOS doesn't have MSG_NOSIGNAL, use SO_NOSIGPIPE instead
// macOS doesn't have MSG_NOSIGNAL, we use SO_NOSIGPIPE instead
#ifndef MSG_NOSIGNAL
#ifdef __APPLE__
#define MSG_NOSIGNAL 0
#else
#define MSG_NOSIGNAL 0x20000  // Linux value
#endif
#endif

namespace rt_stt {
namespace ipc {

struct Server::Impl {
    std::string socket_path;
    int server_socket = -1;
    std::thread accept_thread;
    
    // Client management
    std::mutex clients_mutex;
    std::map<int, std::unique_ptr<ClientInfo>> clients;
    int next_client_id = 1;
    
    // Socket cleanup on destruction
    ~Impl() {
        if (server_socket >= 0) {
            close(server_socket);
        }
        if (!socket_path.empty()) {
            unlink(socket_path.c_str());
        }
    }
};

Server::Server() : impl_(std::make_unique<Impl>()) {}

Server::~Server() {
    shutdown();
}

bool Server::initialize(const std::string& socket_path) {
    if (impl_->server_socket >= 0) {
        std::cerr << "Server already initialized" << std::endl;
        return false;
    }
    
    impl_->socket_path = socket_path;
    
    // Create Unix domain socket
    impl_->server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (impl_->server_socket < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Remove existing socket file
    unlink(socket_path.c_str());
    
    // Bind socket
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);
    
    if (bind(impl_->server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
        close(impl_->server_socket);
        impl_->server_socket = -1;
        return false;
    }
    
    // Listen for connections
    if (listen(impl_->server_socket, 10) < 0) {
        std::cerr << "Failed to listen on socket: " << strerror(errno) << std::endl;
        close(impl_->server_socket);
        impl_->server_socket = -1;
        unlink(socket_path.c_str());
        return false;
    }
    
    std::cout << "IPC server initialized on " << socket_path << std::endl;
    return true;
}

bool Server::start() {
    if (running_.load()) {
        return true;
    }
    
    if (impl_->server_socket < 0) {
        std::cerr << "Server not initialized" << std::endl;
        return false;
    }
    
    running_ = true;
    shutdown_requested_ = false;
    
    // Start accept thread
    impl_->accept_thread = std::thread(&Server::accept_connections, this);
    
    std::cout << "IPC server started" << std::endl;
    return true;
}

void Server::stop() {
    if (!running_.load()) {
        return;
    }
    
    shutdown_requested_ = true;
    
    // Close server socket to wake up accept()
    if (impl_->server_socket >= 0) {
        ::shutdown(impl_->server_socket, SHUT_RDWR);
    }
    
    // Wait for accept thread
    if (impl_->accept_thread.joinable()) {
        impl_->accept_thread.join();
    }
    
    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(impl_->clients_mutex);
        for (auto& [fd, client] : impl_->clients) {
            close(fd);
            if (client->handler_thread.joinable()) {
                client->handler_thread.join();
            }
        }
        impl_->clients.clear();
    }
    
    running_ = false;
    std::cout << "IPC server stopped" << std::endl;
}

void Server::shutdown() {
    stop();
    
    if (impl_->server_socket >= 0) {
        close(impl_->server_socket);
        impl_->server_socket = -1;
    }
    
    if (!impl_->socket_path.empty()) {
        unlink(impl_->socket_path.c_str());
        impl_->socket_path.clear();
    }
}

void Server::accept_connections() {
    while (!shutdown_requested_.load()) {
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(impl_->server_socket, 
                             (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            if (shutdown_requested_.load()) {
                break;
            }
            std::cerr << "Failed to accept connection: " << strerror(errno) << std::endl;
            continue;
        }
        
        // On macOS, disable SIGPIPE on the socket
#ifdef __APPLE__
        int nosigpipe = 1;
        setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
#endif
        
        // Create client info
        auto client = std::make_unique<ClientInfo>();
        client->socket_fd = client_fd;
        client->id = "client_" + std::to_string(impl_->next_client_id++);
        client->subscribed_to_transcriptions = true; // Default to subscribed
        
        // Start client handler thread
        client->handler_thread = std::thread(&Server::handle_client, this, client_fd);
        
        // Store client
        {
            std::lock_guard<std::mutex> lock(impl_->clients_mutex);
            impl_->clients[client_fd] = std::move(client);
        }
        
        std::cout << "Client connected: fd=" << client_fd << std::endl;
    }
}

void Server::handle_client(int client_fd) {
    while (!shutdown_requested_.load()) {
        Message msg;
        if (!receive_message(client_fd, msg)) {
            break; // Client disconnected
        }
        
        process_message(client_fd, msg);
    }
    
    cleanup_client(client_fd);
}

bool Server::send_message(int client_fd, const Message& msg) {
    try {
        nlohmann::json json_msg = {
            {"type", msg.type},
            {"id", msg.id},
            {"data", msg.data}
        };
        
        std::string serialized = json_msg.dump();
        
        // Send with length prefix for proper framing (not including newline)
        uint32_t length = htonl(serialized.length());
        if (send(client_fd, &length, sizeof(length), MSG_NOSIGNAL) != sizeof(length)) {
            return false;
        }
        
        // Send message
        size_t total_sent = 0;
        while (total_sent < serialized.length()) {
            ssize_t sent = send(client_fd, serialized.c_str() + total_sent, 
                              serialized.length() - total_sent, MSG_NOSIGNAL);
            if (sent <= 0) {
                return false;
            }
            total_sent += sent;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to send message: " << e.what() << std::endl;
        return false;
    }
}

bool Server::receive_message(int client_fd, Message& msg) {
    try {
        // Read length prefix
        uint32_t length_prefix;
        if (recv(client_fd, &length_prefix, sizeof(length_prefix), MSG_WAITALL) != sizeof(length_prefix)) {
            return false;
        }
        
        uint32_t length = ntohl(length_prefix);
        if (length > 1024 * 1024) { // Max 1MB message
            std::cerr << "Message too large: " << length << std::endl;
            return false;
        }
        
        // Read message
        std::vector<char> buffer(length);
        size_t total_received = 0;
        while (total_received < length) {
            ssize_t received = recv(client_fd, buffer.data() + total_received,
                                  length - total_received, 0);
            if (received <= 0) {
                return false;
            }
            total_received += received;
        }
        
        // Parse JSON
        std::string json_str(buffer.data(), length);
        auto json_msg = nlohmann::json::parse(json_str);
        
        msg.type = static_cast<Message::Type>(json_msg["type"].get<int>());
        msg.id = json_msg.value("id", "");
        msg.data = json_msg["data"];
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to receive message: " << e.what() << std::endl;
        return false;
    }
}

void Server::process_message(int client_fd, const Message& msg) {
    switch (msg.type) {
        case Message::COMMAND: {
            if (command_handler_) {
                std::string action = msg.data.value("action", "");
                auto params = msg.data.value("params", nlohmann::json::object());
                
                try {
                    auto result = command_handler_(action, params);
                    
                    Message response;
                    response.type = Message::ACKNOWLEDGMENT;
                    response.id = msg.id;
                    response.data = {
                        {"success", true},
                        {"result", result}
                    };
                    send_message(client_fd, response);
                } catch (const std::exception& e) {
                    Message error;
                    error.type = Message::ERROR;
                    error.id = msg.id;
                    error.data = {
                        {"message", e.what()}
                    };
                    send_message(client_fd, error);
                }
            }
            break;
        }
        
        case Message::SUBSCRIBE: {
            std::lock_guard<std::mutex> lock(impl_->clients_mutex);
            if (auto it = impl_->clients.find(client_fd); it != impl_->clients.end()) {
                it->second->subscribed_to_transcriptions = true;
                
                Message ack;
                ack.type = Message::ACKNOWLEDGMENT;
                ack.id = msg.id;
                ack.data = {{"subscribed", true}};
                send_message(client_fd, ack);
            }
            break;
        }
        
        case Message::UNSUBSCRIBE: {
            std::lock_guard<std::mutex> lock(impl_->clients_mutex);
            if (auto it = impl_->clients.find(client_fd); it != impl_->clients.end()) {
                it->second->subscribed_to_transcriptions = false;
                
                Message ack;
                ack.type = Message::ACKNOWLEDGMENT;
                ack.id = msg.id;
                ack.data = {{"subscribed", false}};
                send_message(client_fd, ack);
            }
            break;
        }
        
        default:
            std::cerr << "Unknown message type: " << msg.type << std::endl;
            break;
    }
}

void Server::broadcast_transcription(const std::string& text, float confidence) {
    Message msg;
    msg.type = Message::TRANSCRIPTION;
    msg.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    msg.data = {
        {"text", text},
        {"confidence", confidence},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
    
    std::lock_guard<std::mutex> lock(impl_->clients_mutex);
    size_t sent_count = 0;
    for (auto& [fd, client] : impl_->clients) {
        if (client->subscribed_to_transcriptions) {
            if (send_message(fd, msg)) {
                sent_count++;
            } else {
                std::cerr << "Failed to send transcription to client " << client->id << std::endl;
            }
        }
    }
    
    if (sent_count == 0 && impl_->clients.size() > 0) {
        std::cerr << "Warning: Transcription not sent to any clients (" << impl_->clients.size() << " connected)" << std::endl;
    }
}

void Server::broadcast_status(const nlohmann::json& status) {
    Message msg;
    msg.type = Message::STATUS;
    msg.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    msg.data = status;
    
    std::lock_guard<std::mutex> lock(impl_->clients_mutex);
    for (auto& [fd, client] : impl_->clients) {
        send_message(fd, msg);
    }
}

void Server::broadcast_transcription_full(const nlohmann::json& transcription_data) {
    Message msg;
    msg.type = Message::TRANSCRIPTION;
    msg.id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    msg.data = transcription_data;
    
    std::lock_guard<std::mutex> lock(impl_->clients_mutex);
    for (auto& [fd, client] : impl_->clients) {
        if (client->subscribed_to_transcriptions) {
            if (!send_message(fd, msg)) {
                std::cerr << "Failed to send full transcription to client " << client->id << std::endl;
            }
        }
    }
}

size_t Server::get_client_count() const {
    std::lock_guard<std::mutex> lock(impl_->clients_mutex);
    return impl_->clients.size();
}

void Server::cleanup_client(int client_fd) {
    {
        std::lock_guard<std::mutex> lock(impl_->clients_mutex);
        
        if (auto it = impl_->clients.find(client_fd); it != impl_->clients.end()) {
            std::cout << "Client disconnected: " << it->second->id << std::endl;
            
            // Note: We don't join the thread here as we're in the thread
            it->second->handler_thread.detach();
            impl_->clients.erase(it);
        }
    }
    
    // Close socket after releasing the lock
    close(client_fd);
}

} // namespace ipc
} // namespace rt_stt