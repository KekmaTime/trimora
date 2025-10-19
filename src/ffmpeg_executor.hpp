#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <filesystem>

namespace trimora {

struct TrimOptions {
    std::filesystem::path input_file;
    std::filesystem::path output_file;
    std::string start_time;  // Format: HH:MM:SS.mmm or seconds
    std::string end_time;    // Format: HH:MM:SS.mmm or seconds
    bool use_copy_codec = true;  // -c copy for fast trimming
};

struct FFmpegProgress {
    double percentage = 0.0;
    std::string current_time;
    std::string fps;
    std::string speed;
};

enum class FFmpegStatus {
    NotStarted,
    Running,
    Completed,
    Failed,
    Cancelled
};

class FFmpegExecutor {
public:
    using ProgressCallback = std::function<void(const FFmpegProgress&)>;
    using StatusCallback = std::function<void(FFmpegStatus, const std::string& message)>;

    FFmpegExecutor();
    ~FFmpegExecutor();

    // Check if FFmpeg is available
    bool is_ffmpeg_available() const;
    std::optional<std::string> get_ffmpeg_path() const;
    std::optional<std::string> get_ffmpeg_version() const;

    // Execute trim operation (blocking)
    bool execute_trim(const TrimOptions& options, std::string& error_message);

    // Execute trim operation (async with callbacks)
    void execute_trim_async(
        const TrimOptions& options,
        ProgressCallback progress_cb,
        StatusCallback status_cb
    );

    // Cancel running operation
    void cancel();

    // Check if operation is running
    bool is_running() const;

private:
    std::string build_ffmpeg_command(const TrimOptions& options) const;
    bool validate_ffmpeg_binary(const std::filesystem::path& path) const;
    FFmpegProgress parse_progress_line(const std::string& line) const;

    std::filesystem::path ffmpeg_path_;
    std::string ffmpeg_version_;
    bool is_running_ = false;
};

} // namespace trimora

