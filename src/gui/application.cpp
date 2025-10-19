#include "application.hpp"
#include "main_window.hpp"
#include "../config_manager.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>
#include <iostream>

namespace trimora {

Application::Application() = default;

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    if (!init_glfw()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    
    if (!init_opengl()) {
        std::cerr << "Failed to initialize OpenGL context" << std::endl;
        return false;
    }
    
    if (!init_imgui()) {
        std::cerr << "Failed to initialize ImGui" << std::endl;
        return false;
    }
    
    // Create config manager
    config_manager_ = std::make_unique<ConfigManager>();
    config_manager_->load();
    
    // Create main window
    main_window_ = std::make_unique<MainWindow>(*config_manager_);
    
    is_running_ = true;
    return true;
}

void Application::run() {
    while (is_running_ && !glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        
        begin_frame();
        
        // Render main window
        main_window_->render();
        
        end_frame();
    }
}

void Application::shutdown() {
    if (main_window_) {
        main_window_.reset();
    }
    
    if (config_manager_) {
        config_manager_->save();
        config_manager_.reset();
    }
    
    // Cleanup ImGui
    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    
    // Cleanup GLFW
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool Application::init_glfw() {
    if (!glfwInit()) {
        return false;
    }
    
    // GL 3.3 + GLSL 330
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    
    window_ = glfwCreateWindow(
        window_width_,
        window_height_,
        window_title_.c_str(),
        nullptr,
        nullptr
    );
    
    if (!window_) {
        return false;
    }
    
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable vsync
    
    return true;
}

bool Application::init_opengl() {
    // No loader needed for basic OpenGL 3.3
    return true;
}

bool Application::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // Setup style
    ImGui::StyleColorsDark();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    return true;
}

void Application::begin_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Application::end_frame() {
    ImGui::Render();
    
    int display_w, display_h;
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glfwSwapBuffers(window_);
}

} // namespace trimora

