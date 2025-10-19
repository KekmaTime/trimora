#include "video_player.hpp"
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cstring>

namespace trimora {

// OpenGL extension function pointers
static PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ = nullptr;
static PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ = nullptr;
static PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers_ = nullptr;
static PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer_ = nullptr;
static PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage_ = nullptr;
static PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer_ = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_ = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers_ = nullptr;
static PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers_ = nullptr;

static void load_gl_extensions() {
    static bool loaded = false;
    if (loaded) return;
    
    glGenFramebuffers_ = (PFNGLGENFRAMEBUFFERSPROC)glfwGetProcAddress("glGenFramebuffers");
    glBindFramebuffer_ = (PFNGLBINDFRAMEBUFFERPROC)glfwGetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D_ = (PFNGLFRAMEBUFFERTEXTURE2DPROC)glfwGetProcAddress("glFramebufferTexture2D");
    glGenRenderbuffers_ = (PFNGLGENRENDERBUFFERSPROC)glfwGetProcAddress("glGenRenderbuffers");
    glBindRenderbuffer_ = (PFNGLBINDRENDERBUFFERPROC)glfwGetProcAddress("glBindRenderbuffer");
    glRenderbufferStorage_ = (PFNGLRENDERBUFFERSTORAGEPROC)glfwGetProcAddress("glRenderbufferStorage");
    glFramebufferRenderbuffer_ = (PFNGLFRAMEBUFFERRENDERBUFFERPROC)glfwGetProcAddress("glFramebufferRenderbuffer");
    glCheckFramebufferStatus_ = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)glfwGetProcAddress("glCheckFramebufferStatus");
    glDeleteFramebuffers_ = (PFNGLDELETEFRAMEBUFFERSPROC)glfwGetProcAddress("glDeleteFramebuffers");
    glDeleteRenderbuffers_ = (PFNGLDELETERENDERBUFFERSPROC)glfwGetProcAddress("glDeleteRenderbuffers");
    
    loaded = true;
}

VideoPlayer::VideoPlayer()
    : mpv_(nullptr)
    , mpv_gl_(nullptr)
    , fbo_(0)
    , fbo_texture_(0)
    , rbo_(0)
    , fbo_width_(0)
    , fbo_height_(0)
    , initialized_(false)
    , has_file_(false)
{
}

VideoPlayer::~VideoPlayer() {
    destroy_fbo();
    
    if (mpv_gl_) {
        mpv_render_context_free(mpv_gl_);
    }
    
    if (mpv_) {
        mpv_terminate_destroy(mpv_);
    }
}

void* VideoPlayer::get_proc_address_mpv(void* /*ctx*/, const char* name) {
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

void VideoPlayer::on_mpv_render_update(void* ctx) {
    // Callback when mpv wants to render
    (void)ctx;
}

void VideoPlayer::on_mpv_events(void* ctx) {
    // Callback for mpv events
    (void)ctx;
}

bool VideoPlayer::initialize() {
    if (initialized_) {
        return true;
    }
    
    // Load OpenGL extensions
    load_gl_extensions();
    
    // Create mpv instance
    mpv_ = mpv_create();
    if (!mpv_) {
        std::cerr << "Failed to create MPV instance" << std::endl;
        return false;
    }
    
    // Set options
    mpv_set_option_string(mpv_, "vo", "libmpv");
    mpv_set_option_string(mpv_, "hwdec", "auto");
    mpv_set_option_string(mpv_, "keep-open", "yes");
    
    // Initialize mpv
    if (mpv_initialize(mpv_) < 0) {
        std::cerr << "Failed to initialize MPV" << std::endl;
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
        return false;
    }
    
    // Setup OpenGL rendering
    mpv_opengl_init_params gl_init_params{get_proc_address_mpv, nullptr};
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    
    if (mpv_render_context_create(&mpv_gl_, mpv_, params) < 0) {
        std::cerr << "Failed to create MPV render context" << std::endl;
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
        return false;
    }
    
    mpv_render_context_set_update_callback(mpv_gl_, on_mpv_render_update, nullptr);
    
    initialized_ = true;
    return true;
}

bool VideoPlayer::load_file(const std::filesystem::path& file_path) {
    if (!initialized_) {
        std::cerr << "VideoPlayer not initialized" << std::endl;
        return false;
    }
    
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "Video file does not exist: " << file_path << std::endl;
        return false;
    }
    
    const char* cmd[] = {"loadfile", file_path.c_str(), nullptr};
    int result = mpv_command(mpv_, cmd);
    
    if (result < 0) {
        std::cerr << "Failed to load file: " << mpv_error_string(result) << std::endl;
        return false;
    }
    
    current_file_ = file_path.string();
    has_file_ = true;
    
    return true;
}

void VideoPlayer::play() {
    if (!initialized_ || !has_file_) return;
    
    int flag = 0;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &flag);
}

void VideoPlayer::pause() {
    if (!initialized_ || !has_file_) return;
    
    int flag = 1;
    mpv_set_property(mpv_, "pause", MPV_FORMAT_FLAG, &flag);
}

void VideoPlayer::toggle_play_pause() {
    if (!initialized_ || !has_file_) return;
    
    int is_paused = 1;
    mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &is_paused);
    
    if (is_paused) {
        play();
    } else {
        pause();
    }
}

void VideoPlayer::stop() {
    if (!initialized_) return;
    
    const char* cmd[] = {"stop", nullptr};
    mpv_command(mpv_, cmd);
    
    has_file_ = false;
}

void VideoPlayer::seek(double position_seconds) {
    if (!initialized_ || !has_file_) return;
    
    mpv_set_property(mpv_, "time-pos", MPV_FORMAT_DOUBLE, &position_seconds);
}

void VideoPlayer::seek_relative(double offset_seconds) {
    if (!initialized_ || !has_file_) return;
    
    double current = get_current_time();
    double new_pos = current + offset_seconds;
    
    if (new_pos < 0) new_pos = 0;
    
    double duration = get_duration();
    if (duration > 0 && new_pos > duration) {
        new_pos = duration;
    }
    
    seek(new_pos);
}

double VideoPlayer::get_duration() const {
    if (!initialized_ || !has_file_) return 0.0;
    
    double duration = 0;
    mpv_get_property(mpv_, "duration", MPV_FORMAT_DOUBLE, &duration);
    return duration;
}

double VideoPlayer::get_current_time() const {
    if (!initialized_ || !has_file_) return 0.0;
    
    double time = 0;
    mpv_get_property(mpv_, "time-pos", MPV_FORMAT_DOUBLE, &time);
    return time;
}

bool VideoPlayer::is_playing() const {
    if (!initialized_ || !has_file_) return false;
    
    int is_paused = 1;
    mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &is_paused);
    return !is_paused;
}

bool VideoPlayer::has_file() const {
    return has_file_;
}

void VideoPlayer::set_volume(double volume) {
    if (!initialized_) return;
    
    mpv_set_property(mpv_, "volume", MPV_FORMAT_DOUBLE, &volume);
}

void VideoPlayer::set_speed(double speed) {
    if (!initialized_) return;
    
    mpv_set_property(mpv_, "speed", MPV_FORMAT_DOUBLE, &speed);
}

void VideoPlayer::create_fbo(int width, int height) {
    if (width == fbo_width_ && height == fbo_height_ && fbo_ != 0) {
        return; // Already created with correct size
    }
    
    destroy_fbo();
    
    fbo_width_ = width;
    fbo_height_ = height;
    
    // Create framebuffer
    glGenFramebuffers_(1, &fbo_);
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo_);
    
    // Create texture
    glGenTextures(1, &fbo_texture_);
    glBindTexture(GL_TEXTURE_2D, fbo_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_texture_, 0);
    
    // Create renderbuffer for depth/stencil
    glGenRenderbuffers_(1, &rbo_);
    glBindRenderbuffer_(GL_RENDERBUFFER, rbo_);
    glRenderbufferStorage_(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo_);
    
    if (glCheckFramebufferStatus_(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer not complete!" << std::endl;
    }
    
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
}

void VideoPlayer::destroy_fbo() {
    if (fbo_) {
        glDeleteFramebuffers_(1, &fbo_);
        fbo_ = 0;
    }
    if (fbo_texture_) {
        glDeleteTextures(1, &fbo_texture_);
        fbo_texture_ = 0;
    }
    if (rbo_) {
        glDeleteRenderbuffers_(1, &rbo_);
        rbo_ = 0;
    }
}

void VideoPlayer::render(int width, int height) {
    if (!initialized_ || !mpv_gl_ || !has_file_) return;
    
    create_fbo(width, height);
    
    mpv_opengl_fbo mpv_fbo{
        static_cast<int>(fbo_),
        width,
        height,
        0
    };
    
    int flip_y = 0;  // Don't flip - ImGui expects the texture right-side up
    
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpv_fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    
    mpv_render_context_render(mpv_gl_, params);
}

} // namespace trimora

