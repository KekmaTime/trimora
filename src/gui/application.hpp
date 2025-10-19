#pragma once

#include <memory>
#include <string>

struct GLFWwindow;

namespace trimora {

class MainWindow;
class ConfigManager;

class Application {
public:
    Application();
    ~Application();

    // Initialize application
    bool initialize();

    // Main application loop
    void run();

    // Shutdown
    void shutdown();

    // Get config manager
    ConfigManager& get_config_manager() { return *config_manager_; }

private:
    bool init_glfw();
    bool init_opengl();
    bool init_imgui();

    void begin_frame();
    void end_frame();

    GLFWwindow* window_ = nullptr;
    std::unique_ptr<MainWindow> main_window_;
    std::unique_ptr<ConfigManager> config_manager_;
    
    bool is_running_ = false;
    
    // Window properties
    int window_width_ = 800;
    int window_height_ = 600;
    std::string window_title_ = "Trimora";
};

} // namespace trimora

