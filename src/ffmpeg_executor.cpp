#include "ffmpeg_executor.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <thread>
#include <atomic>
#include <cstdlib>
#include <array>
#include <memory>
#include <algorithm>

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
    
    // Calculate target duration for progress tracking
    double start_seconds = parse_time_to_seconds(options.start_time);
    double end_seconds = parse_time_to_seconds(options.end_time);
    double target_duration = end_seconds - start_seconds;
    (void)target_duration;  // Unused in blocking version
    
    is_running_ = true;
    
    try {
        // Build command string with progress output
        std::ostringstream cmd;
        cmd << ffmpeg_path_.string() << " ";
        cmd << "-y ";  // Overwrite output files without asking
        cmd << "-progress pipe:1 ";  // Output progress to stdout
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
        
        // Read output line by line
        std::array<char, 512> buffer;
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr && is_running_) {
            std::string line(buffer.data());
            
            // Print all output for debugging
            std::cout << line;
            
            // Parse progress (FFmpeg progress format or stderr format)
            // Progress will be handled in async version with callbacks
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
        
        if (!is_ffmpeg_available()) {
            status_cb(FFmpegStatus::Failed, "FFmpeg not found in PATH");
            return;
        }
        
        if (!fs::exists(options.input_file)) {
            status_cb(FFmpegStatus::Failed, "Input file does not exist");
            return;
        }
        
        // Create output directory if it doesn't exist
        auto output_dir = options.output_file.parent_path();
        if (!output_dir.empty() && !fs::exists(output_dir)) {
            try {
                fs::create_directories(output_dir);
            } catch (const std::exception& e) {
                status_cb(FFmpegStatus::Failed, "Failed to create output directory: " + std::string(e.what()));
                return;
            }
        }
        
        // Calculate target duration
        double start_seconds = parse_time_to_seconds(options.start_time);
        double end_seconds = parse_time_to_seconds(options.end_time);
        double target_duration = end_seconds - start_seconds;
        
        is_running_ = true;
        
        try {
            // Build command with progress output
            std::ostringstream cmd;
            cmd << ffmpeg_path_.string() << " ";
            cmd << "-y ";
            cmd << "-progress pipe:1 ";
            cmd << "-ss " << options.start_time << " ";
            cmd << "-to " << options.end_time << " ";
            cmd << "-i \"" << options.input_file.string() << "\" ";
            
            if (options.use_copy_codec) {
                cmd << "-c copy ";
            }
            
            cmd << "\"" << options.output_file.string() << "\" ";
            cmd << "2>&1";
            
            FILE* pipe = popen(cmd.str().c_str(), "r");
            if (!pipe) {
                is_running_ = false;
                status_cb(FFmpegStatus::Failed, "Failed to execute FFmpeg");
                return;
            }
            
            // Read and parse output
            std::array<char, 1024> buffer;
            std::string accumulated_line;
            
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr && is_running_) {
                std::string line(buffer.data());
                accumulated_line += line;
                
                // Check if we have a complete line
                if (line.find('\n') != std::string::npos) {
                    // Parse progress
                    auto progress = parse_progress_line(accumulated_line, target_duration);
                    if (progress.percentage > 0) {
                        progress_cb(progress);
                    }
                    accumulated_line.clear();
                }
            }
            
            int exit_code = pclose(pipe);
            is_running_ = false;
            
            if (exit_code != 0) {
                status_cb(FFmpegStatus::Failed, "FFmpeg exited with code: " + std::to_string(exit_code));
            } else {
                // Final progress update
                FFmpegProgress final_progress;
                final_progress.percentage = 100.0;
                progress_cb(final_progress);
                status_cb(FFmpegStatus::Completed, "Trim completed successfully");
            }
            
        } catch (const std::exception& e) {
            is_running_ = false;
            status_cb(FFmpegStatus::Failed, std::string("Exception: ") + e.what());
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

double FFmpegExecutor::parse_time_to_seconds(const std::string& time_str) const {
    // Try decimal seconds first
    try {
        return std::stod(time_str);
    } catch (...) {
        // Fall through to HH:MM:SS.mmm parsing
    }
    
    // Parse HH:MM:SS.mmm format
    std::regex time_regex(R"((\d{2}):(\d{2}):(\d{2})\.?(\d{0,3}))");
    std::smatch match;
    
    if (std::regex_match(time_str, match, time_regex)) {
        int hours = std::stoi(match[1]);
        int minutes = std::stoi(match[2]);
        int seconds = std::stoi(match[3]);
        int milliseconds = match[4].length() > 0 ? std::stoi(match[4]) : 0;
        
        return hours * 3600.0 + minutes * 60.0 + seconds + milliseconds / 1000.0;
    }
    
    return 0.0;
}

double FFmpegExecutor::get_video_duration(const fs::path& video_path) const {
    // Use ffprobe to get duration
    std::ostringstream cmd;
    cmd << "ffprobe -v error -show_entries format=duration ";
    cmd << "-of default=noprint_wrappers=1:nokey=1 ";
    cmd << "\"" << video_path.string() << "\" 2>&1";
    
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        return 0.0;
    }
    
    std::array<char, 128> buffer;
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    
    try {
        return std::stod(result);
    } catch (...) {
        return 0.0;
    }
}

FFmpegProgress FFmpegExecutor::parse_progress_line(const std::string& line, double total_duration) const {
    FFmpegProgress progress;
    
    // FFmpeg progress format (with -progress):
    // out_time_us=1234567890
    // or stderr format:
    // frame= 1234 fps=30 q=-1.0 Lsize= 1024kB time=00:01:23.45 bitrate= 104.3kbits/s speed=1.5x
    
    // Try progress format first (out_time_us)
    std::regex out_time_regex(R"(out_time_us=(\d+))");
    std::smatch match;
    
    if (std::regex_search(line, match, out_time_regex)) {
        long long microseconds = std::stoll(match[1]);
        double current_seconds = microseconds / 1000000.0;
        
        if (total_duration > 0) {
            progress.percentage = std::min(100.0, (current_seconds / total_duration) * 100.0);
        }
        
        // Format time
        int hours = static_cast<int>(current_seconds / 3600);
        int minutes = static_cast<int>((current_seconds - hours * 3600) / 60);
        int seconds = static_cast<int>(current_seconds - hours * 3600 - minutes * 60);
        
        std::ostringstream time_ss;
        time_ss << std::setfill('0') << std::setw(2) << hours << ":"
                << std::setw(2) << minutes << ":"
                << std::setw(2) << seconds;
        progress.current_time = time_ss.str();
        
        return progress;
    }
    
    // Try stderr format (frame= ... time=...)
    std::regex time_regex(R"(time=(\d{2}:\d{2}:\d{2}\.\d{2}))");
    std::regex fps_regex(R"(fps=\s*(\d+\.?\d*))");
    std::regex speed_regex(R"(speed=\s*(\d+\.?\d*)x)");
    
    if (std::regex_search(line, match, time_regex)) {
        progress.current_time = match[1];
        
        // Calculate percentage from time
        double current_seconds = parse_time_to_seconds(progress.current_time);
        if (total_duration > 0) {
            progress.percentage = std::min(100.0, (current_seconds / total_duration) * 100.0);
        }
    }
    
    if (std::regex_search(line, match, fps_regex)) {
        progress.fps = match[1];
    }
    
    if (std::regex_search(line, match, speed_regex)) {
        progress.speed = match[1].str() + "x";
    }
    
    return progress;
}

} // namespace trimora

