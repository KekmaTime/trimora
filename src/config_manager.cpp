#include "config_manager.hpp"
#include "file_manager.hpp"
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace trimora {

ConfigManager::ConfigManager() {
    config_file_path_ = get_config_file_path();
    load_defaults();
}

bool ConfigManager::load() {
    if (!fs::exists(config_file_path_)) {
        // Create default config
        load_defaults();
        return save();
    }
    
    try {
        std::ifstream file(config_file_path_);
        if (!file) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        
        return parse_json(buffer.str());
    } catch (...) {
        return false;
    }
}

bool ConfigManager::save() const {
    try {
        // Ensure config directory exists
        auto config_dir = config_file_path_.parent_path();
        if (!fs::exists(config_dir)) {
            fs::create_directories(config_dir);
        }
        
        std::ofstream file(config_file_path_);
        if (!file) {
            return false;
        }
        
        file << to_json();
        return true;
    } catch (...) {
        return false;
    }
}

void ConfigManager::set_ffmpeg_path(const fs::path& path) {
    config_.ffmpeg_path = path;
}

void ConfigManager::set_output_directory(const fs::path& path) {
    config_.output_directory = path;
}

void ConfigManager::set_output_naming_pattern(const std::string& pattern) {
    config_.output_naming_pattern = pattern;
}

fs::path ConfigManager::get_config_file_path() {
    return FileManager::get_config_dir() / "config.json";
}

void ConfigManager::load_defaults() {
    config_.ffmpeg_path = "/usr/bin/ffmpeg";
    config_.output_directory = FileManager::get_default_output_dir();
    config_.output_naming_pattern = "{name}_trimmed_{timestamp}";
    config_.recent_files_count = 5;
    config_.auto_open_output = false;
    config_.log_level = "info";
    config_.theme = "dark";
}

bool ConfigManager::parse_json(const std::string& /*json_content*/) {
    // Simple JSON parsing (for production, use a proper JSON library)
    // This is a simplified implementation
    
    // For now, just load defaults and return true
    // In production, parse the JSON properly
    load_defaults();
    return true;
}

std::string ConfigManager::to_json() const {
    std::ostringstream json;
    
    json << "{\n";
    json << "  \"ffmpeg_path\": \"" << config_.ffmpeg_path.string() << "\",\n";
    json << "  \"output_directory\": \"" << config_.output_directory.string() << "\",\n";
    json << "  \"output_naming_pattern\": \"" << config_.output_naming_pattern << "\",\n";
    json << "  \"recent_files_count\": " << config_.recent_files_count << ",\n";
    json << "  \"auto_open_output\": " << (config_.auto_open_output ? "true" : "false") << ",\n";
    json << "  \"log_level\": \"" << config_.log_level << "\",\n";
    json << "  \"theme\": \"" << config_.theme << "\"\n";
    json << "}\n";
    
    return json.str();
}

} // namespace trimora

