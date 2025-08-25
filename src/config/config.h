#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace rt_stt {
namespace config {

struct Config {
    std::string model_path = "models/ggml-base.en.bin";
    std::string language = "en";
    int threads = 4;
    bool gpu_enabled = true;
};

class ConfigManager {
public:
    ConfigManager() = default;
    ~ConfigManager() = default;
    
    bool load(const std::string& path) { return true; }
    bool save(const std::string& path) { return true; }
    
    Config get() const { return config_; }
    void set(const Config& config) { config_ = config; }
    
private:
    Config config_;
};

} // namespace config
} // namespace rt_stt

#endif // CONFIG_H
