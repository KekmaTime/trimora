cmake_minimum_required(VERSION 3.20)
project(trimora VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(TRIMORA_BUILD_TESTS "Build unit tests" ON)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Find dependencies
find_package(Boost 1.70 REQUIRED COMPONENTS system filesystem)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(X11 REQUIRED)

# ImGui as submodule or vendored
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
if(NOT EXISTS ${IMGUI_DIR})
    message(FATAL_ERROR "ImGui not found in external/imgui. Please clone it or add as submodule.")
endif()

# ImGui sources
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Source files
set(TRIMORA_SOURCES
    src/main.cpp
    src/ffmpeg_executor.cpp
    src/file_manager.cpp
    src/validator.cpp
    src/config_manager.cpp
    src/gui/application.cpp
    src/gui/main_window.cpp
)

set(TRIMORA_HEADERS
    src/ffmpeg_executor.hpp
    src/file_manager.hpp
    src/validator.hpp
    src/config_manager.hpp
    src/gui/application.hpp
    src/gui/main_window.hpp
)

# Create executable
add_executable(trimora 
    ${TRIMORA_SOURCES}
    ${IMGUI_SOURCES}
)

target_include_directories(trimora PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(trimora PRIVATE
    Boost::system
    Boost::filesystem
    OpenGL::GL
    glfw
    ${X11_LIBRARIES}
    ${CMAKE_DL_LIBS}
)

# Platform-specific settings
if(UNIX AND NOT APPLE)
    target_link_libraries(trimora PRIVATE pthread)
elseif(WIN32)
    target_compile_definitions(trimora PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Install rules
install(TARGETS trimora DESTINATION bin)

# Tests
if(TRIMORA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

