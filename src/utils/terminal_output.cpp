#include "utils/terminal_output.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>

#ifdef __APPLE__
#include <termios.h>
#include <unistd.h>
#endif

namespace rt_stt {
namespace utils {

TerminalOutput::TerminalOutput() 
    : colored_output_(true)
    , raw_mode_(false)
    , is_speaking_(false)
    , saved_termios_(nullptr) {
    // Check if output is to a terminal
    colored_output_ = isatty(STDOUT_FILENO);
}

TerminalOutput::~TerminalOutput() {
    if (raw_mode_) {
        disable_raw_mode();
    }
}

void TerminalOutput::enable_raw_mode() {
#ifdef __APPLE__
    if (!raw_mode_ && isatty(STDIN_FILENO)) {
        termios* old_termios = new termios;
        tcgetattr(STDIN_FILENO, old_termios);
        saved_termios_ = old_termios;
        
        termios new_termios = *old_termios;
        new_termios.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
        
        raw_mode_ = true;
    }
#endif
}

void TerminalOutput::disable_raw_mode() {
#ifdef __APPLE__
    if (raw_mode_ && saved_termios_) {
        tcsetattr(STDIN_FILENO, TCSANOW, static_cast<termios*>(saved_termios_));
        delete static_cast<termios*>(saved_termios_);
        saved_termios_ = nullptr;
        raw_mode_ = false;
    }
#endif
}

void TerminalOutput::set_colored_output(bool enabled) {
    colored_output_ = enabled && isatty(STDOUT_FILENO);
}

void TerminalOutput::print_transcript(const std::string& text, float confidence, bool is_final) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    
    std::cout << "\r" << std::string(100, ' ') << "\r";
    
    if (is_final) {
        std::cout << color_green() << "[FINAL]" << color_reset() << " ";
    } else {
        std::cout << color_yellow() << "[PARTIAL]" << color_reset() << " ";
    }
    
    std::cout << text;
    
    // Show confidence as a visual bar
    std::cout << " " << color_dim() << "[";
    int conf_bars = static_cast<int>(confidence * 10);
    for (int i = 0; i < 10; ++i) {
        if (i < conf_bars) {
            std::cout << "=";
        } else {
            std::cout << " ";
        }
    }
    std::cout << "]" << color_reset();
    
    if (is_final) {
        std::cout << std::endl;
    } else {
        std::cout << std::flush;
    }
}

void TerminalOutput::print_audio_level(float level) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    
    // Only update every 100ms to reduce flicker
    auto now = std::chrono::steady_clock::now();
    if (now - last_update_ < std::chrono::milliseconds(100)) {
        return;
    }
    last_update_ = now;
    
    // Save cursor position
    std::cout << "\033[s";
    
    // Move to bottom of terminal
    std::cout << "\033[999;1H";
    
    // Clear line and print audio level
    std::cout << "\033[K";
    std::cout << color_blue() << "Audio: " << color_reset();
    
    // Convert to dB for better visualization
    float db = 20.0f * std::log10(std::max(0.0001f, level));
    db = std::max(-60.0f, std::min(0.0f, db));
    
    // Draw VU meter
    int meter_width = 40;
    int filled = static_cast<int>((db + 60.0f) / 60.0f * meter_width);
    
    std::cout << "[";
    for (int i = 0; i < meter_width; ++i) {
        if (i < filled) {
            if (db > -20.0f) {
                std::cout << color_red() << "#" << color_reset();
            } else if (db > -40.0f) {
                std::cout << color_yellow() << "#" << color_reset();
            } else {
                std::cout << color_green() << "#" << color_reset();
            }
        } else {
            std::cout << "-";
        }
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << db << " dB";
    
    // Restore cursor position
    std::cout << "\033[u" << std::flush;
}

void TerminalOutput::print_status(const std::string& status) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    std::cout << color_blue() << "[STATUS]" << color_reset() << " " << status << std::endl;
}

void TerminalOutput::print_error(const std::string& error) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    std::cout << color_red() << "[ERROR]" << color_reset() << " " << error << std::endl;
}

void TerminalOutput::print_vad_status(bool is_speaking) {
    if (is_speaking_.exchange(is_speaking) != is_speaking) {
        std::lock_guard<std::mutex> lock(output_mutex_);
        if (is_speaking) {
            std::cout << color_green() << "🎤 Speech detected" << color_reset() << std::endl;
        } else {
            std::cout << color_dim() << "🔇 Silence" << color_reset() << std::endl;
        }
    }
}

void TerminalOutput::print_latency(std::chrono::milliseconds latency) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    
    std::string color;
    if (latency.count() < 100) {
        color = color_green();
    } else if (latency.count() < 200) {
        color = color_yellow();
    } else {
        color = color_red();
    }
    
    std::cout << color_dim() << "[Latency: " << color << latency.count() << "ms" 
              << color_dim() << "]" << color_reset() << std::endl;
}

void TerminalOutput::update_metrics(float cpu_usage, size_t memory_mb, int active_threads) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    
    // Save cursor position
    std::cout << "\033[s";
    
    // Move to top right corner
    std::cout << "\033[1;60H";
    
    // Print metrics
    std::cout << color_dim() << "CPU: " << color_reset() 
              << std::fixed << std::setprecision(1) << cpu_usage << "%";
    
    std::cout << "\033[2;60H";
    std::cout << color_dim() << "MEM: " << color_reset() << memory_mb << "MB";
    
    std::cout << "\033[3;60H";
    std::cout << color_dim() << "THR: " << color_reset() << active_threads;
    
    // Restore cursor position
    std::cout << "\033[u" << std::flush;
}

void TerminalOutput::clear_line() {
    std::lock_guard<std::mutex> lock(output_mutex_);
    std::cout << "\r" << std::string(100, ' ') << "\r" << std::flush;
}

void TerminalOutput::clear_screen() {
    std::lock_guard<std::mutex> lock(output_mutex_);
    std::cout << "\033[2J\033[H" << std::flush;
}

void TerminalOutput::move_cursor_up(int lines) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    std::cout << "\033[" << lines << "A" << std::flush;
}

// Color methods
std::string TerminalOutput::color_reset() const {
    return colored_output_ ? "\033[0m" : "";
}

std::string TerminalOutput::color_green() const {
    return colored_output_ ? "\033[32m" : "";
}

std::string TerminalOutput::color_yellow() const {
    return colored_output_ ? "\033[33m" : "";
}

std::string TerminalOutput::color_red() const {
    return colored_output_ ? "\033[31m" : "";
}

std::string TerminalOutput::color_blue() const {
    return colored_output_ ? "\033[34m" : "";
}

std::string TerminalOutput::color_dim() const {
    return colored_output_ ? "\033[2m" : "";
}

std::string TerminalOutput::color_bold() const {
    return colored_output_ ? "\033[1m" : "";
}

} // namespace utils
} // namespace rt_stt
