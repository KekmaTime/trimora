#include "file_manager.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace trimora {

fs::path FileManager::generate_output_filename(
    const fs::path& input_file,
    const fs::path& output_dir,
    const std::string& pattern
) {
    std::string filename = pattern;
    
    // Replace {name} with input filename (without extension)
    auto input_stem = input_file.stem().string();
    size_t name_pos = filename.find("{name}");
    if (name_pos != std::string::npos) {
        filename.replace(name_pos, 6, input_stem);
    }
    
    // Replace {timestamp} with current timestamp
    auto timestamp = get_timestamp_string();
    size_t time_pos = filename.find("{timestamp}");
    if (time_pos != std::string::npos) {
        filename.replace(time_pos, 11, timestamp);
    }
    
    // Add extension
    filename += input_file.extension().string();
    
    // Combine with output directory
    auto output_path = output_dir / filename;
    
    // Handle duplicates
    int counter = 1;
    while (fs::exists(output_path)) {
        std::ostringstream oss;
        oss << input_stem << "_trimmed_" << timestamp << "_" << counter;
        oss << input_file.extension().string();
        output_path = output_dir / oss.str();
        counter++;
    }
    
    return output_path;
}

bool FileManager::should_overwrite(const fs::path& /*path*/) {
    // For now, never overwrite (handled by generate_output_filename)
    return false;
}

std::optional<size_t> FileManager::get_file_size(const fs::path& path) {
    try {
        if (!fs::exists(path)) {
            return std::nullopt;
        }
        return fs::file_size(path);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<size_t> FileManager::get_available_space(const fs::path& path) {
    try {
        auto space_info = fs::space(path);
        return space_info.available;
    } catch (...) {
        return std::nullopt;
    }
}

void FileManager::add_recent_file(const fs::path& path) {
    auto recent_path = recent_files_path();
    
    // Read existing recent files
    std::vector<std::string> recent_files;
    std::ifstream infile(recent_path);
    if (infile) {
        std::string line;
        while (std::getline(infile, line)) {
            if (!line.empty() && line != path.string()) {
                recent_files.push_back(line);
            }
        }
    }
    
    // Add new file at the beginning
    recent_files.insert(recent_files.begin(), path.string());
    
    // Keep only last 10 files
    if (recent_files.size() > 10) {
        recent_files.resize(10);
    }
    
    // Write back
    std::ofstream outfile(recent_path);
    for (const auto& file : recent_files) {
        outfile << file << "\n";
    }
}

std::vector<fs::path> FileManager::get_recent_files(size_t max_count) {
    std::vector<fs::path> recent_files;
    
    auto recent_path = recent_files_path();
    std::ifstream infile(recent_path);
    if (!infile) {
        return recent_files;
    }
    
    std::string line;
    while (std::getline(infile, line) && recent_files.size() < max_count) {
        if (!line.empty() && fs::exists(line)) {
            recent_files.push_back(line);
        }
    }
    
    return recent_files;
}

void FileManager::clear_recent_files() {
    auto recent_path = recent_files_path();
    fs::remove(recent_path);
}

fs::path FileManager::get_config_dir() {
    fs::path config_dir;
    
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata) {
        config_dir = fs::path(appdata) / "Trimora";
    }
#elif __APPLE__
    const char* home = std::getenv("HOME");
    if (home) {
        config_dir = fs::path(home) / "Library" / "Application Support" / "Trimora";
    }
#else // Linux
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    if (xdg_config) {
        config_dir = fs::path(xdg_config) / "trimora";
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            config_dir = fs::path(home) / ".config" / "trimora";
        }
    }
#endif
    
    // Create directory if it doesn't exist
    if (!config_dir.empty() && !fs::exists(config_dir)) {
        fs::create_directories(config_dir);
    }
    
    return config_dir;
}

fs::path FileManager::get_default_output_dir() {
#ifdef _WIN32
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) {
        return fs::path(userprofile) / "Videos" / "Trimmed";
    }
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return fs::path(home) / "Videos" / "Trimmed";
    }
#endif
    
    return fs::current_path();
}

std::string FileManager::get_timestamp_string() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
    return oss.str();
}

fs::path FileManager::recent_files_path() {
    return get_config_dir() / "recent_files.txt";
}

} // namespace trimora

