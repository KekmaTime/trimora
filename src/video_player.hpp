#pragma once

#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <string>
#include <filesystem>
#include <functional>

namespace trimora {

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    // Initialize the player with OpenGL context
    bool initialize();

    // Load a video file
    bool load_file(const std::filesystem::path& file_path);

    // Playback controls
    void play();
    void pause();
    void toggle_play_pause();
    void stop();
    void seek(double position_seconds);
    void seek_relative(double offset_seconds);

    // Video properties
    double get_duration() const;
    double get_current_time() const;
    bool is_playing() const;
    bool has_file() const;

    // Rendering
    void render(int width, int height);
    unsigned int get_texture_id() const { return fbo_texture_; }

    // Volume and speed
    void set_volume(double volume); // 0-100
    void set_speed(double speed);   // 0.25 - 4.0

private:
    void create_fbo(int width, int height);
    void destroy_fbo();
    static void* get_proc_address_mpv(void* ctx, const char* name);
    static void on_mpv_render_update(void* ctx);
    static void on_mpv_events(void* ctx);

    mpv_handle* mpv_;
    mpv_render_context* mpv_gl_;
    
    unsigned int fbo_;
    unsigned int fbo_texture_;
    unsigned int rbo_;
    int fbo_width_;
    int fbo_height_;
    
    bool initialized_;
    bool has_file_;
    std::string current_file_;
};

} // namespace trimora

