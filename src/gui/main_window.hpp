#pragma once

#include "../ffmpeg_executor.hpp"
#include "../config_manager.hpp"
#include "../video_player.hpp"
#include "../trim_segment.hpp"
#include <string>
#include <memory>
#include <vector>
#include <mutex>

namespace trimora {

class MainWindow {
public:
    explicit MainWindow(ConfigManager& config_manager);
    ~MainWindow();

    // Render the main window
    void render();

private:
    void render_input_section();
    void render_video_player();
    void render_time_inputs();
    void render_segment_mode();
    void render_segment_list();
    void render_batch_mode();
    void render_control_buttons();
    void render_log_console();
    void render_recent_files();

    // Actions
    void browse_input_file();
    void browse_input_files_batch();
    void browse_output_directory();
    void start_trim();
    void start_batch_trim();
    void start_segment_trim();
    void process_next_batch_file();
    void stop_trim();

    // Callbacks
    void on_progress_update(const FFmpegProgress& progress);
    void on_status_update(FFmpegStatus status, const std::string& message);

    // Validation
    bool validate_inputs(std::string& error_message);

    ConfigManager& config_manager_;
    std::unique_ptr<FFmpegExecutor> ffmpeg_executor_;
    std::unique_ptr<VideoPlayer> video_player_;
    std::unique_ptr<SegmentManager> segment_manager_;

    // UI state
    char input_file_[512] = "";
    char output_dir_[512] = "";
    char start_time_[32] = "00:00:00.000";
    char end_time_[32] = "00:00:00.000";
    
    bool is_trimming_ = false;
    float current_progress_ = 0.0f;
    
    // Batch mode
    bool batch_mode_ = false;
    std::vector<std::string> batch_files_;
    size_t current_batch_index_ = 0;
    size_t total_batch_count_ = 0;
    
    // Multi-segment mode
    bool segment_mode_ = false;
    int selected_segment_index_ = -1;
    char segment_name_buffer_[128] = "";
    bool merge_segments_ = true;
    
    // Video player state
    bool show_player_ = false;
    float player_volume_ = 100.0f;
    float player_speed_ = 1.0f;
    float seek_position_ = 0.0f;
    
    // Log buffer
    std::vector<std::string> log_messages_;
    std::mutex log_mutex_;
    bool auto_scroll_log_ = true;
    
    // Recent files
    std::vector<std::string> recent_files_;
};

} // namespace trimora

