#include "main_window.hpp"
#include "../file_manager.hpp"
#include "../validator.hpp"

#include <imgui.h>
#include <nfd.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace trimora {

MainWindow::MainWindow(ConfigManager& config_manager)
    : config_manager_(config_manager)
{
    // Initialize NFD
    NFD_Init();
    
    ffmpeg_executor_ = std::make_unique<FFmpegExecutor>();
    
    // Initialize output directory from config
    auto output_dir = config_manager_.get_config().output_directory.string();
    std::strncpy(output_dir_, output_dir.c_str(), sizeof(output_dir_) - 1);
    
    // Load recent files
    auto recent = FileManager::get_recent_files(5);
    for (const auto& file : recent) {
        recent_files_.push_back(file.string());
    }
}

MainWindow::~MainWindow() {
    NFD_Quit();
}

void MainWindow::render() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    
    ImGui::Begin("Trimora", nullptr, 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_MenuBar
    );
    
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Exit")) {
                // TODO: Signal application to exit
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                // TODO: Show about dialog
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    
    // Check FFmpeg availability
    if (!ffmpeg_executor_->is_ffmpeg_available()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 
            "Warning: FFmpeg not found in PATH!");
        ImGui::Text("Please install FFmpeg or configure its path in settings.");
        ImGui::Separator();
    } else {
        auto version = ffmpeg_executor_->get_ffmpeg_version();
        if (version) {
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 
                "FFmpeg: %s", version->c_str());
        }
    }
    
    ImGui::Spacing();
    
    // Main content
    render_input_section();
    render_time_inputs();
    render_batch_mode();
    render_control_buttons();
    
    ImGui::Separator();
    
    render_log_console();
    render_recent_files();
    
    ImGui::End();
}

void MainWindow::render_input_section() {
    if (!batch_mode_) {
        ImGui::Text("Input File:");
        ImGui::PushItemWidth(-100);
        ImGui::InputText("##input", input_file_, sizeof(input_file_));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Browse...##input")) {
            browse_input_file();
        }
    } else {
        ImGui::Text("Batch Mode - %zu file(s) selected", batch_files_.size());
        if (ImGui::Button("Add Files...##batch")) {
            browse_input_files_batch();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear All##batch")) {
            batch_files_.clear();
            std::lock_guard<std::mutex> lock(log_mutex_);
            log_messages_.push_back("Batch list cleared.");
        }
        
        // Show batch file list
        if (!batch_files_.empty()) {
            ImGui::BeginChild("BatchFileList", ImVec2(0, 80), true);
            for (size_t i = 0; i < batch_files_.size(); ++i) {
                std::string label = std::to_string(i + 1) + ". " + batch_files_[i];
                ImGui::Text("%s", label.c_str());
            }
            ImGui::EndChild();
        }
    }
    
    ImGui::Text("Output Directory:");
    ImGui::PushItemWidth(-100);
    ImGui::InputText("##output", output_dir_, sizeof(output_dir_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Browse...##output")) {
        browse_output_directory();
    }
    
    ImGui::Spacing();
}

void MainWindow::render_time_inputs() {
    ImGui::Text("Time Range (HH:MM:SS.mmm):");
    
    ImGui::Text("Start Time:");
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    ImGui::InputText("##start", start_time_, sizeof(start_time_));
    ImGui::PopItemWidth();
    
    ImGui::SameLine();
    ImGui::Text("End Time:");
    ImGui::SameLine();
    ImGui::PushItemWidth(200);
    ImGui::InputText("##end", end_time_, sizeof(end_time_));
    ImGui::PopItemWidth();
    
    ImGui::Spacing();
}

void MainWindow::render_batch_mode() {
    ImGui::Checkbox("Batch Mode", &batch_mode_);
    ImGui::SameLine();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Trim multiple videos with the same time range");
    }
    
    if (batch_mode_ && !batch_files_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), 
            "(%zu files ready)", batch_files_.size());
    }
    
    ImGui::Spacing();
}

void MainWindow::render_control_buttons() {
    bool can_trim = false;
    
    if (!batch_mode_) {
        can_trim = ffmpeg_executor_->is_ffmpeg_available() && 
                   !is_trimming_ && 
                   strlen(input_file_) > 0;
    } else {
        can_trim = ffmpeg_executor_->is_ffmpeg_available() && 
                   !is_trimming_ && 
                   !batch_files_.empty();
    }
    
    if (!can_trim) {
        ImGui::BeginDisabled();
    }
    
    if (batch_mode_) {
        if (ImGui::Button("Trim All Videos", ImVec2(150, 30))) {
            start_batch_trim();
        }
    } else {
        if (ImGui::Button("Trim Video", ImVec2(120, 30))) {
            start_trim();
        }
    }
    
    if (!can_trim) {
        ImGui::EndDisabled();
    }
    
    ImGui::SameLine();
    
    if (!is_trimming_) {
        ImGui::BeginDisabled();
    }
    
    if (ImGui::Button("Stop", ImVec2(80, 30))) {
        stop_trim();
    }
    
    if (!is_trimming_) {
        ImGui::EndDisabled();
    }
    
    if (is_trimming_) {
        ImGui::SameLine();
        
        // Format progress text
        char progress_text[128];
        if (batch_mode_ && total_batch_count_ > 0) {
            snprintf(progress_text, sizeof(progress_text), 
                "File %zu/%zu - %.1f%%", 
                current_batch_index_ + 1, 
                total_batch_count_, 
                current_progress_ * 100.0f);
        } else {
            snprintf(progress_text, sizeof(progress_text), "%.1f%%", current_progress_ * 100.0f);
        }
        ImGui::ProgressBar(current_progress_, ImVec2(-1, 0), progress_text);
    }
    
    ImGui::Spacing();
}

void MainWindow::render_log_console() {
    ImGui::Text("Log Console:");
    
    ImGui::BeginChild("LogConsole", ImVec2(0, 150), true);
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    for (const auto& msg : log_messages_) {
        ImGui::TextUnformatted(msg.c_str());
    }
    
    if (auto_scroll_log_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }
    
    ImGui::EndChild();
}

void MainWindow::render_recent_files() {
    if (recent_files_.empty()) {
        return;
    }
    
    ImGui::Spacing();
    ImGui::Text("Recent Files:");
    
    for (size_t i = 0; i < recent_files_.size() && i < 5; ++i) {
        if (ImGui::Selectable(recent_files_[i].c_str())) {
            std::strncpy(input_file_, recent_files_[i].c_str(), sizeof(input_file_) - 1);
        }
    }
}

void MainWindow::browse_input_file() {
    nfdchar_t* out_path = nullptr;
    nfdfilteritem_t filters[1] = {{"Video Files", "mp4,mkv,avi,mov,webm"}};
    
    nfdresult_t result = NFD_OpenDialog(&out_path, filters, 1, nullptr);
    
    if (result == NFD_OKAY) {
        std::strncpy(input_file_, out_path, sizeof(input_file_) - 1);
        input_file_[sizeof(input_file_) - 1] = '\0';
        
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Selected: " + std::string(out_path));
        
        NFD_FreePath(out_path);
    } else if (result == NFD_CANCEL) {
        // User cancelled, do nothing
    } else {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Error: " + std::string(NFD_GetError()));
    }
}

void MainWindow::browse_input_files_batch() {
    const nfdpathset_t* path_set = nullptr;
    nfdfilteritem_t filters[1] = {{"Video Files", "mp4,mkv,avi,mov,webm"}};
    
    nfdresult_t result = NFD_OpenDialogMultiple(&path_set, filters, 1, nullptr);
    
    if (result == NFD_OKAY) {
        nfdpathsetsize_t count = 0;
        NFD_PathSet_GetCount(path_set, &count);
        
        for (nfdpathsetsize_t i = 0; i < count; ++i) {
            nfdchar_t* path = nullptr;
            NFD_PathSet_GetPath(path_set, i, &path);
            if (path) {
                batch_files_.push_back(std::string(path));
                NFD_PathSet_FreePath(path);
            }
        }
        
        NFD_PathSet_Free(path_set);
        
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Added " + std::to_string(count) + " files to batch list.");
    } else if (result == NFD_CANCEL) {
        // User cancelled, do nothing
    } else {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Error: " + std::string(NFD_GetError()));
    }
}

void MainWindow::browse_output_directory() {
    nfdchar_t* out_path = nullptr;
    
    nfdresult_t result = NFD_PickFolder(&out_path, nullptr);
    
    if (result == NFD_OKAY) {
        std::strncpy(output_dir_, out_path, sizeof(output_dir_) - 1);
        output_dir_[sizeof(output_dir_) - 1] = '\0';
        
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Output directory: " + std::string(out_path));
        
        NFD_FreePath(out_path);
    } else if (result == NFD_CANCEL) {
        // User cancelled, do nothing
    } else {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Error: " + std::string(NFD_GetError()));
    }
}

void MainWindow::start_trim() {
    std::string error_msg;
    if (!validate_inputs(error_msg)) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Error: " + error_msg);
        return;
    }
    
    is_trimming_ = true;
    current_progress_ = 0.0f;
    
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Starting trim operation...");
    }
    
    // Build options
    TrimOptions options;
    options.input_file = input_file_;
    
    // Generate output filename
    options.output_file = FileManager::generate_output_filename(
        options.input_file,
        output_dir_,
        config_manager_.get_config().output_naming_pattern
    );
    
    options.start_time = start_time_;
    options.end_time = end_time_;
    options.use_copy_codec = true;
    
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Input: " + options.input_file.string());
        log_messages_.push_back("Output: " + options.output_file.string());
        log_messages_.push_back("Time range: " + options.start_time + " to " + options.end_time);
    }
    
    // Execute async
    ffmpeg_executor_->execute_trim_async(
        options,
        [this](const FFmpegProgress& progress) {
            on_progress_update(progress);
        },
        [this](FFmpegStatus status, const std::string& message) {
            on_status_update(status, message);
        }
    );
}

void MainWindow::start_batch_trim() {
    if (batch_files_.empty()) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Error: No files in batch list.");
        return;
    }
    
    // Validate time range
    std::string error_msg;
    auto time_validation = Validator::validate_time_range(start_time_, end_time_);
    if (!time_validation) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Error: " + time_validation.error_message);
        return;
    }
    
    current_batch_index_ = 0;
    total_batch_count_ = batch_files_.size();
    is_trimming_ = true;
    current_progress_ = 0.0f;
    
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("=== Starting batch trim of " + 
            std::to_string(total_batch_count_) + " files ===");
    }
    
    process_next_batch_file();
}

void MainWindow::process_next_batch_file() {
    if (current_batch_index_ >= batch_files_.size()) {
        // All done
        is_trimming_ = false;
        current_batch_index_ = 0;
        total_batch_count_ = 0;
        
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("=== Batch trim completed! ===");
        return;
    }
    
    // Get current file
    std::string current_file = batch_files_[current_batch_index_];
    
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_messages_.push_back("Processing file " + 
            std::to_string(current_batch_index_ + 1) + "/" + 
            std::to_string(total_batch_count_) + ": " + current_file);
    }
    
    // Build options
    TrimOptions options;
    options.input_file = current_file;
    
    // Generate output filename
    options.output_file = FileManager::generate_output_filename(
        options.input_file,
        output_dir_,
        config_manager_.get_config().output_naming_pattern
    );
    
    options.start_time = start_time_;
    options.end_time = end_time_;
    options.use_copy_codec = true;
    
    current_progress_ = 0.0f;
    
    // Execute async
    ffmpeg_executor_->execute_trim_async(
        options,
        [this](const FFmpegProgress& progress) {
            on_progress_update(progress);
        },
        [this](FFmpegStatus status, const std::string& message) {
            if (status == FFmpegStatus::Completed) {
                std::lock_guard<std::mutex> lock(log_mutex_);
                log_messages_.push_back("✓ File " + 
                    std::to_string(current_batch_index_ + 1) + " completed.");
                
                // Move to next file
                current_batch_index_++;
                process_next_batch_file();
            } else if (status == FFmpegStatus::Failed) {
                std::lock_guard<std::mutex> lock(log_mutex_);
                log_messages_.push_back("✗ File " + 
                    std::to_string(current_batch_index_ + 1) + " failed: " + message);
                
                // Move to next file anyway
                current_batch_index_++;
                process_next_batch_file();
            } else {
                on_status_update(status, message);
            }
        }
    );
}

void MainWindow::stop_trim() {
    ffmpeg_executor_->cancel();
    is_trimming_ = false;
    
    // Reset batch state
    if (batch_mode_) {
        current_batch_index_ = 0;
        total_batch_count_ = 0;
    }
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_messages_.push_back("Trim operation cancelled.");
}

void MainWindow::on_progress_update(const FFmpegProgress& progress) {
    current_progress_ = static_cast<float>(progress.percentage / 100.0);
    
    // Log progress updates
    if (!progress.current_time.empty() || progress.percentage > 0) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        
        std::ostringstream msg;
        msg << "Progress: " << std::fixed << std::setprecision(1) << progress.percentage << "%";
        
        if (!progress.current_time.empty()) {
            msg << " | Time: " << progress.current_time;
        }
        if (!progress.speed.empty()) {
            msg << " | Speed: " << progress.speed;
        }
        
        // Only add if it's a significant update (avoid spam)
        if (log_messages_.empty() || 
            log_messages_.back().find("Progress:") == std::string::npos ||
            static_cast<int>(progress.percentage) % 10 == 0) {
            log_messages_.push_back(msg.str());
        } else {
            // Replace last progress message
            if (!log_messages_.empty() && log_messages_.back().find("Progress:") != std::string::npos) {
                log_messages_.back() = msg.str();
            }
        }
    }
}

void MainWindow::on_status_update(FFmpegStatus status, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    switch (status) {
        case FFmpegStatus::Running:
            log_messages_.push_back("Status: Running - " + message);
            break;
        case FFmpegStatus::Completed:
            log_messages_.push_back("Success: " + message);
            is_trimming_ = false;
            current_progress_ = 1.0f;
            FileManager::add_recent_file(input_file_);
            break;
        case FFmpegStatus::Failed:
            log_messages_.push_back("Error: " + message);
            is_trimming_ = false;
            current_progress_ = 0.0f;
            break;
        case FFmpegStatus::Cancelled:
            log_messages_.push_back("Cancelled: " + message);
            is_trimming_ = false;
            current_progress_ = 0.0f;
            break;
        default:
            break;
    }
}

bool MainWindow::validate_inputs(std::string& error_message) {
    // Validate input file
    if (strlen(input_file_) == 0) {
        error_message = "Please select an input file";
        return false;
    }
    
    auto input_validation = Validator::validate_input_file(input_file_);
    if (!input_validation) {
        error_message = input_validation.error_message;
        return false;
    }
    
    // Validate output directory
    if (strlen(output_dir_) == 0) {
        error_message = "Please select an output directory";
        return false;
    }
    
    // Validate timestamps
    auto time_validation = Validator::validate_time_range(start_time_, end_time_);
    if (!time_validation) {
        error_message = time_validation.error_message;
        return false;
    }
    
    return true;
}

} // namespace trimora

