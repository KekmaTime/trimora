#include "ffmpeg_executor.hpp"
#include <iostream>
#include <sstream>
#include <regex>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <array>
#include <memory>

namespace fs = std::filesystem;

namespace trimora {

FFmpegExecutor::FFmpegExecutor() {
    // Try to find ffmpeg in PATH - simple check
    // Check common locations
    std::vector<fs::path> search_paths = {
        "/usr/bin/ffmpeg",
        "/usr/local/bin/ffmpeg",
        "/bin/ffmpeg"
    };
    
    for (const auto& path : search_paths) {
        if (fs::exists(path)) {
            ffmpeg_path_ = path;
            break;
        }
    }
    
    // Try using 'which' command as fallback
    if (ffmpeg_path_.empty()) {
        FILE* pipe = popen("which ffmpeg", "r");
        if (pipe) {
            std::array<char, 512> buffer;
            std::string result;
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                result += buffer.data();
            }
            pclose(pipe);
            
            // Remove trailing newline
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
            
            if (!result.empty() && fs::exists(result)) {
                ffmpeg_path_ = result;
            }
        }
    }
    
    // Get version if found
    if (!ffmpeg_path_.empty()) {
        FILE* pipe = popen((ffmpeg_path_.string() + " -version 2>&1").c_str(), "r");
        if (pipe) {
            std::array<char, 512> buffer;
            if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
                ffmpeg_version_ = buffer.data();
                // Remove trailing newline
                if (!ffmpeg_version_.empty() && ffmpeg_version_.back() == '\n') {
                    ffmpeg_version_.pop_back();
                }
            }
            pclose(pipe);
        }
    }
}

FFmpegExecutor::~FFmpegExecutor() {
    cancel();
}

bool FFmpegExecutor::is_ffmpeg_available() const {
    return !ffmpeg_path_.empty() && fs::exists(ffmpeg_path_);
}

std::optional<std::string> FFmpegExecutor::get_ffmpeg_path() const {
    if (ffmpeg_path_.empty()) {
        return std::nullopt;
    }
    return ffmpeg_path_.string();
}

std::optional<std::string> FFmpegExecutor::get_ffmpeg_version() const {
    if (ffmpeg_version_.empty()) {
        return std::nullopt;
    }
    return ffmpeg_version_;
}

std::string FFmpegExecutor::build_ffmpeg_command(const TrimOptions& options) const {
    std::ostringstream cmd;
    
    // FFmpeg command: ffmpeg -ss START -to END -i INPUT -c copy OUTPUT
    cmd << ffmpeg_path_.string() << " ";
    cmd << "-ss " << options.start_time << " ";
    cmd << "-to " << options.end_time << " ";
    cmd << "-i \"" << options.input_file.string() << "\" ";
    
    if (options.use_copy_codec) {
        cmd << "-c copy ";
    }
    
    cmd << "\"" << options.output_file.string() << "\"";
    
    return cmd.str();
}

bool FFmpegExecutor::execute_trim(const TrimOptions& options, std::string& error_message) {
    if (!is_ffmpeg_available()) {
        error_message = "FFmpeg not found in PATH";
        return false;
    }
    
    if (!fs::exists(options.input_file)) {
        error_message = "Input file does not exist: " + options.input_file.string();
        return false;
    }
    
    // Create output directory if it doesn't exist
    auto output_dir = options.output_file.parent_path();
    if (!output_dir.empty() && !fs::exists(output_dir)) {
        try {
            fs::create_directories(output_dir);
        } catch (const std::exception& e) {
            error_message = "Failed to create output directory: " + std::string(e.what());
            return false;
        }
    }
    
    is_running_ = true;
    
    try {
        // Build command string
        std::ostringstream cmd;
        cmd << ffmpeg_path_.string() << " ";
        cmd << "-y ";  // Overwrite output files without asking
        cmd << "-ss " << options.start_time << " ";
        cmd << "-to " << options.end_time << " ";
        cmd << "-i \"" << options.input_file.string() << "\" ";
        
        if (options.use_copy_codec) {
            cmd << "-c copy ";
        }
        
        cmd << "\"" << options.output_file.string() << "\" ";
        cmd << "2>&1";  // Redirect stderr to stdout
        
        std::cout << "Executing: " << cmd.str() << std::endl;
        
        // Execute command
        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) {
            is_running_ = false;
            error_message = "Failed to execute FFmpeg command";
            return false;
        }
        
        // Read output
        std::array<char, 512> buffer;
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            std::cout << buffer.data();
        }
        
        int exit_code = pclose(pipe);
        is_running_ = false;
        
        if (exit_code != 0) {
            error_message = "FFmpeg exited with code: " + std::to_string(exit_code);
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        is_running_ = false;
        error_message = std::string("Exception: ") + e.what();
        return false;
    }
}

void FFmpegExecutor::execute_trim_async(
    const TrimOptions& options,
    ProgressCallback progress_cb,
    StatusCallback status_cb
) {
    // Launch in separate thread
    std::thread worker([this, options, progress_cb, status_cb]() {
        status_cb(FFmpegStatus::Running, "Starting FFmpeg...");
        
        std::string error_message;
        bool success = execute_trim(options, error_message);
        
        if (success) {
            status_cb(FFmpegStatus::Completed, "Trim completed successfully");
        } else {
            status_cb(FFmpegStatus::Failed, error_message);
        }
    });
    
    worker.detach();
}

void FFmpegExecutor::cancel() {
    if (is_running_) {
        // TODO: Implement proper process cancellation
        is_running_ = false;
    }
}

bool FFmpegExecutor::is_running() const {
    return is_running_;
}

bool FFmpegExecutor::validate_ffmpeg_binary(const fs::path& path) const {
    if (!fs::exists(path)) {
        return false;
    }
    
    // Check if executable
    auto perms = fs::status(path).permissions();
    return (perms & fs::perms::owner_exec) != fs::perms::none;
}

FFmpegProgress FFmpegExecutor::parse_progress_line(const std::string& line) const {
    FFmpegProgress progress;
    
    // FFmpeg progress format: frame= 1234 fps=30 q=-1.0 Lsize= 1024kB time=00:01:23.45 bitrate= 104.3kbits/s speed=1.5x
    std::regex time_regex(R"(time=(\d{2}:\d{2}:\d{2}\.\d{2}))");
    std::regex fps_regex(R"(fps=\s*(\d+\.?\d*))");
    std::regex speed_regex(R"(speed=\s*(\d+\.?\d*)x)");
    
    std::smatch match;
    if (std::regex_search(line, match, time_regex)) {
        progress.current_time = match[1];
    }
    if (std::regex_search(line, match, fps_regex)) {
        progress.fps = match[1];
    }
    if (std::regex_search(line, match, speed_regex)) {
        progress.speed = match[1];
    }
    
    return progress;
}

} // namespace trimora

