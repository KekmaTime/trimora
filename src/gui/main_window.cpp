#include "main_window.hpp"
#include "../file_manager.hpp"
#include "../validator.hpp"

#include <imgui.h>
#include <nfd.h>
#include <iostream>
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
    render_control_buttons();
    
    ImGui::Separator();
    
    render_log_console();
    render_recent_files();
    
    ImGui::End();
}

void MainWindow::render_input_section() {
    ImGui::Text("Input File:");
    ImGui::PushItemWidth(-100);
    ImGui::InputText("##input", input_file_, sizeof(input_file_));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Browse...##input")) {
        browse_input_file();
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

void MainWindow::render_control_buttons() {
    bool can_trim = ffmpeg_executor_->is_ffmpeg_available() && 
                    !is_trimming_ && 
                    strlen(input_file_) > 0;
    
    if (!can_trim) {
        ImGui::BeginDisabled();
    }
    
    if (ImGui::Button("Trim Video", ImVec2(120, 30))) {
        start_trim();
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
        ImGui::ProgressBar(current_progress_, ImVec2(-1, 0));
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

void MainWindow::stop_trim() {
    ffmpeg_executor_->cancel();
    is_trimming_ = false;
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_messages_.push_back("Trim operation cancelled.");
}

void MainWindow::on_progress_update(const FFmpegProgress& progress) {
    current_progress_ = static_cast<float>(progress.percentage / 100.0);
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

