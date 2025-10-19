#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <optional>

namespace trimora {

class FileManager {
public:
    // Generate output filename with pattern
    static std::filesystem::path generate_output_filename(
        const std::filesystem::path& input_file,
        const std::filesystem::path& output_dir,
        const std::string& pattern = "{name}_trimmed_{timestamp}"
    );

    // Check if file exists and handle overwrite
    static bool should_overwrite(const std::filesystem::path& path);

    // Get file size
    static std::optional<size_t> get_file_size(const std::filesystem::path& path);

    // Get available disk space
    static std::optional<size_t> get_available_space(const std::filesystem::path& path);

    // Recent files management
    static void add_recent_file(const std::filesystem::path& path);
    static std::vector<std::filesystem::path> get_recent_files(size_t max_count = 5);
    static void clear_recent_files();

    // Platform-specific path handling
    static std::filesystem::path get_config_dir();
    static std::filesystem::path get_default_output_dir();

private:
    static std::string get_timestamp_string();
    static std::filesystem::path recent_files_path();
};

} // namespace trimora

