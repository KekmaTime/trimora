#pragma once

#include <string>
#include <filesystem>
#include <optional>

namespace trimora {

struct Config {
    std::filesystem::path ffmpeg_path = "/usr/bin/ffmpeg";
    std::filesystem::path output_directory;
    std::string output_naming_pattern = "{name}_trimmed_{timestamp}";
    size_t recent_files_count = 5;
    bool auto_open_output = false;
    std::string log_level = "info";
    std::string theme = "dark";
};

class ConfigManager {
public:
    ConfigManager();

    // Load/Save config
    bool load();
    bool save() const;

    // Getters
    const Config& get_config() const { return config_; }
    Config& get_config() { return config_; }

    // Setters for common options
    void set_ffmpeg_path(const std::filesystem::path& path);
    void set_output_directory(const std::filesystem::path& path);
    void set_output_naming_pattern(const std::string& pattern);

    // Get config file path
    static std::filesystem::path get_config_file_path();

private:
    Config config_;
    std::filesystem::path config_file_path_;

    void load_defaults();
    bool parse_json(const std::string& json_content);
    std::string to_json() const;
};

} // namespace trimora

