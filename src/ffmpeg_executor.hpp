#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <filesystem>
#include "trim_segment.hpp"

namespace trimora {

struct TrimOptions {
    std::filesystem::path input_file;
    std::filesystem::path output_file;
    std::string start_time;  // Format: HH:MM:SS.mmm or seconds
    std::string end_time;    // Format: HH:MM:SS.mmm or seconds
    bool use_copy_codec = true;  // -c copy for fast trimming
};

struct MultiSegmentTrimOptions {
    std::filesystem::path input_file;
    std::filesystem::path output_file;  // Base name for output
    std::vector<TrimSegment> segments;
    bool merge_segments = true;  // Merge into one file or create separate files
    bool use_copy_codec = true;
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

    // Execute multi-segment trim (async with callbacks)
    void execute_multi_segment_trim_async(
        const MultiSegmentTrimOptions& options,
        ProgressCallback progress_cb,
        StatusCallback status_cb
    );

    // Cancel running operation
    void cancel();

    // Check if operation is running
    bool is_running() const;

private:
    std::string build_ffmpeg_command(const TrimOptions& options) const;
    std::string build_multi_segment_command(const MultiSegmentTrimOptions& options) const;
    std::string build_concat_command(
        const std::vector<std::filesystem::path>& segment_files,
        const std::filesystem::path& output_file
    ) const;
    bool validate_ffmpeg_binary(const std::filesystem::path& path) const;
    FFmpegProgress parse_progress_line(const std::string& line, double total_duration) const;
    double get_video_duration(const std::filesystem::path& video_path) const;
    double parse_time_to_seconds(const std::string& time_str) const;

    std::filesystem::path ffmpeg_path_;
    std::string ffmpeg_version_;
    bool is_running_ = false;
};

} // namespace trimora

