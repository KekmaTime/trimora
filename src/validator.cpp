#include "validator.hpp"
#include <regex>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace trimora {

const std::vector<char> Validator::dangerous_chars_ = {';', '&', '|', '$', '`', '\n', '\r'};

ValidationResult Validator::validate_timestamp(const std::string& timestamp) {
    ValidationResult result;
    
    if (timestamp.empty()) {
        result.error = ValidationError::InvalidTimestamp;
        result.error_message = "Timestamp cannot be empty";
        return result;
    }
    
    // Check format: HH:MM:SS.mmm or decimal seconds
    std::regex time_regex(R"(^(\d{2}):(\d{2}):(\d{2})\.(\d{3})$)");
    std::regex decimal_regex(R"(^\d+\.?\d*$)");
    
    if (!std::regex_match(timestamp, time_regex) && 
        !std::regex_match(timestamp, decimal_regex)) {
        result.error = ValidationError::InvalidTimestamp;
        result.error_message = "Invalid timestamp format. Use HH:MM:SS.mmm or decimal seconds";
        return result;
    }
    
    // Validate ranges for HH:MM:SS format
    if (std::regex_match(timestamp, time_regex)) {
        std::smatch match;
        std::regex_search(timestamp, match, time_regex);
        
        // Hours can be any value
        int minutes = std::stoi(match[2]);
        int seconds = std::stoi(match[3]);
        
        if (minutes >= 60 || seconds >= 60) {
            result.error = ValidationError::InvalidTimestamp;
            result.error_message = "Invalid time values (minutes/seconds must be < 60)";
            return result;
        }
    }
    
    result.is_valid = true;
    return result;
}

ValidationResult Validator::validate_time_range(const std::string& start, const std::string& end) {
    auto start_result = validate_timestamp(start);
    if (!start_result) {
        return start_result;
    }
    
    auto end_result = validate_timestamp(end);
    if (!end_result) {
        return end_result;
    }
    
    auto start_seconds = timestamp_to_seconds(start);
    auto end_seconds = timestamp_to_seconds(end);
    
    if (!start_seconds || !end_seconds) {
        ValidationResult result;
        result.error = ValidationError::InvalidTimestamp;
        result.error_message = "Failed to convert timestamps to seconds";
        return result;
    }
    
    if (*start_seconds >= *end_seconds) {
        ValidationResult result;
        result.error = ValidationError::StartTimeAfterEndTime;
        result.error_message = "Start time must be less than end time";
        return result;
    }
    
    ValidationResult result;
    result.is_valid = true;
    return result;
}

std::optional<double> Validator::timestamp_to_seconds(const std::string& timestamp) {
    std::regex time_regex(R"(^(\d{2}):(\d{2}):(\d{2})\.(\d{3})$)");
    std::smatch match;
    
    if (std::regex_match(timestamp, match, time_regex)) {
        int hours = std::stoi(match[1]);
        int minutes = std::stoi(match[2]);
        int seconds = std::stoi(match[3]);
        int milliseconds = std::stoi(match[4]);
        
        double total = hours * 3600.0 + minutes * 60.0 + seconds + milliseconds / 1000.0;
        return total;
    }
    
    // Try decimal format
    try {
        return std::stod(timestamp);
    } catch (...) {
        return std::nullopt;
    }
}

ValidationResult Validator::validate_input_file(const fs::path& path) {
    ValidationResult result;
    
    if (!fs::exists(path)) {
        result.error = ValidationError::FileNotFound;
        result.error_message = "File not found: " + path.string();
        return result;
    }
    
    if (!fs::is_regular_file(path)) {
        result.error = ValidationError::InvalidFormat;
        result.error_message = "Path is not a regular file: " + path.string();
        return result;
    }
    
    // Check if readable
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        result.error = ValidationError::FileNotReadable;
        result.error_message = "File is not readable: " + path.string();
        return result;
    }
    
    result.is_valid = true;
    return result;
}

ValidationResult Validator::validate_output_path(const fs::path& path) {
    ValidationResult result;
    
    auto parent = path.parent_path();
    
    if (!fs::exists(parent)) {
        result.error = ValidationError::OutputNotWritable;
        result.error_message = "Output directory does not exist: " + parent.string();
        return result;
    }
    
    // Check if parent directory is writable (simplified check)
    auto perms = fs::status(parent).permissions();
    if ((perms & fs::perms::owner_write) == fs::perms::none) {
        result.error = ValidationError::OutputNotWritable;
        result.error_message = "Output directory is not writable: " + parent.string();
        return result;
    }
    
    result.is_valid = true;
    return result;
}

bool Validator::is_valid_mp4(const fs::path& path) {
    if (!fs::exists(path)) {
        return false;
    }
    
    // Simple check: read first 12 bytes for ftyp box
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    
    char buffer[12];
    file.read(buffer, 12);
    
    if (file.gcount() < 12) {
        return false;
    }
    
    // Check for 'ftyp' signature (at offset 4-7)
    return buffer[4] == 'f' && buffer[5] == 't' && 
           buffer[6] == 'y' && buffer[7] == 'p';
}

std::string Validator::sanitize_filename(const std::string& filename) {
    std::string sanitized = filename;
    
    // Replace dangerous characters with underscores
    for (char& c : sanitized) {
        if (std::find(dangerous_chars_.begin(), dangerous_chars_.end(), c) != dangerous_chars_.end()) {
            c = '_';
        }
    }
    
    return sanitized;
}

bool Validator::contains_dangerous_chars(const std::string& path) {
    return std::any_of(dangerous_chars_.begin(), dangerous_chars_.end(),
        [&path](char c) { return path.find(c) != std::string::npos; });
}

bool Validator::has_sufficient_disk_space(const fs::path& output_dir, size_t required_bytes) {
    try {
        auto space_info = fs::space(output_dir);
        return space_info.available >= required_bytes;
    } catch (...) {
        return false;
    }
}

} // namespace trimora

