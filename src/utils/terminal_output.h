#ifndef TERMINAL_OUTPUT_H
#define TERMINAL_OUTPUT_H

#include <string>
#include <mutex>
#include <chrono>
#include <atomic>

namespace rt_stt {
namespace utils {

class TerminalOutput {
public:
    TerminalOutput();
    ~TerminalOutput();

    // Configure output modes
    void enable_raw_mode();
    void disable_raw_mode();
    void set_colored_output(bool enabled);
    
    // Real-time output methods
    void print_transcript(const std::string& text, float confidence, bool is_final);
    void print_audio_level(float level);
    void print_status(const std::string& status);
    void print_error(const std::string& error);
    void print_vad_status(bool is_speaking);
    void print_latency(std::chrono::milliseconds latency);
    
    // Performance metrics display
    void update_metrics(float cpu_usage, size_t memory_mb, int active_threads);
    
    // Clear and refresh
    void clear_line();
    void clear_screen();
    void move_cursor_up(int lines);
    
private:
    std::mutex output_mutex_;
    bool colored_output_;
    bool raw_mode_;
    std::atomic<bool> is_speaking_;
    std::chrono::steady_clock::time_point last_update_;
    
    // Terminal control sequences
    std::string color_reset() const;
    std::string color_green() const;
    std::string color_yellow() const;
    std::string color_red() const;
    std::string color_blue() const;
    std::string color_dim() const;
    std::string color_bold() const;
    
    // Save/restore terminal state
    void* saved_termios_;
};

} // namespace utils
} // namespace rt_stt

#endif // TERMINAL_OUTPUT_H
