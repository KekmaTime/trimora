#pragma once

#include <string>
#include <filesystem>
#include <optional>
#include <vector>

namespace trimora {

enum class ValidationError {
    None,
    FileNotFound,
    FileNotReadable,
    InvalidFormat,
    InvalidTimestamp,
    StartTimeAfterEndTime,
    OutputNotWritable,
    InsufficientDiskSpace,
    PathContainsDangerousChars
};

struct ValidationResult {
    bool is_valid = false;
    ValidationError error = ValidationError::None;
    std::string error_message;

    operator bool() const { return is_valid; }
};

class Validator {
public:
    // Timestamp validation
    static ValidationResult validate_timestamp(const std::string& timestamp);
    static ValidationResult validate_time_range(const std::string& start, const std::string& end);
    
    // Convert timestamp to seconds
    static std::optional<double> timestamp_to_seconds(const std::string& timestamp);
    
    // File validation
    static ValidationResult validate_input_file(const std::filesystem::path& path);
    static ValidationResult validate_output_path(const std::filesystem::path& path);
    
    // Check if file is valid MP4
    static bool is_valid_mp4(const std::filesystem::path& path);
    
    // Path sanitization
    static std::string sanitize_filename(const std::string& filename);
    static bool contains_dangerous_chars(const std::string& path);
    
    // Disk space check
    static bool has_sufficient_disk_space(
        const std::filesystem::path& output_dir,
        size_t required_bytes
    );
    
private:
    static bool is_timestamp_format_valid(const std::string& timestamp);
    static const std::vector<char> dangerous_chars_;
};

} // namespace trimora

