#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include <string>
#include <functional>
#include <memory>

namespace rt_stt {
namespace ipc {

class Server {
public:
    Server() = default;
    ~Server() = default;
    
    bool initialize(const std::string& socket_path) { return true; }
    void shutdown() {}
    bool start() { return true; }
    void stop() {}
};

} // namespace ipc
} // namespace rt_stt

#endif // IPC_SERVER_H
