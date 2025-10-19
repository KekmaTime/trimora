# Trimora Pro

A lightweight, cross-platform desktop application for trimming MP4 videos with an intuitive GUI, powered by FFmpeg.

## Features

- 🎬 **Simple Video Trimming**: Trim videos by specifying start and end times
- 📺 **Video Preview Player**: Built-in video player with playback controls (powered by libmpv)
  - Play/pause/seek controls
  - Visual timeline scrubbing
  - "Set Start/End Here" buttons for visual trim points
  - Volume and playback speed controls
- ⚡ **Fast Processing**: Uses FFmpeg's stream copy for quick, lossless trimming
- 🎯 **User-Friendly GUI**: Clean interface built with Dear ImGui
- 📊 **Real-time Progress**: Live progress bar with percentage, time, and speed metrics
- 📦 **Batch Mode**: Trim multiple videos with the same time range in one go
- ✅ **Input Validation**: Comprehensive validation of timestamps and file paths
- 📝 **Smart Logging**: See FFmpeg output and progress updates in real-time
- 🛑 **Cancellation Support**: Stop operations at any time
- 📂 **Native File Dialogs**: Browse files and folders with your system's native dialogs
- 📁 **Recent Files**: Quick access to recently trimmed videos
- ⚙️ **Configurable**: JSON-based configuration for paths and settings
- 🗂️ **Auto-create Directories**: Output directories are created automatically if they don't exist

## Prerequisites

### System Requirements
- **OS**: Linux (Arch/Ubuntu), macOS 11+
- **Compiler**: g++ 11+ or clang++ 13+ with C++20 support
- **CMake**: 3.20 or higher
- **FFmpeg**: 4.4 or higher (must be in PATH)
- **libmpv**: For video playback preview

### Dependencies
- Boost (system, filesystem, process)
- OpenGL 3.3+
- GLFW3
- Dear ImGui (automatically cloned during setup)

### Installing Dependencies

**Arch Linux:**
```bash
sudo pacman -S cmake ninja boost glfw-x11 ffmpeg mpv
```

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install cmake ninja-build libboost-all-dev libglfw3-dev ffmpeg libmpv-dev
```

**macOS:**
```bash
brew install cmake ninja boost glfw ffmpeg mpv
```

## Building

### Quick Start

```bash
# Clone the repository
git clone <repository-url>
cd trimora

# Run setup script (Linux/macOS)
chmod +x setup.sh
./setup.sh

# Build
cd build
ninja  # or 'make -j$(nproc)' if ninja not available

# Run
./trimora
```

### Manual Build

```bash
# Clone ImGui
mkdir -p external
git clone --depth 1 --branch docking https://github.com/ocornut/imgui.git external/imgui

# Create build directory
mkdir build && cd build

# Configure
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
ninja

# Run
./trimora
```

## Usage

### Basic Workflow

#### Single File Mode
1. **Launch Trimora**: Run `./trimora` from the build directory
2. **Select Input**: Click "Browse..." to select your MP4 file
3. **Preview Video** (optional): Check "Show Player" to preview and visually select trim points
   - Use play/pause and seek controls
   - Click "Set Start Here" / "Set End Here" to mark trim points
4. **Set Output**: Choose the output directory
5. **Set Time Range**: Enter start and end times in `HH:MM:SS.mmm` format (or use visual markers)
6. **Trim**: Click "Trim Video" and watch the progress
7. **Done**: The trimmed video will be saved with a timestamp

#### Batch Mode
1. **Enable Batch Mode**: Check the "Batch Mode" checkbox
2. **Add Files**: Click "Add Files..." and select multiple videos (Ctrl+Click or Shift+Click)
3. **Set Time Range**: Enter the same start and end times for all videos
4. **Trim All**: Click "Trim All Videos" 
5. **Watch Progress**: See each file processing with "File X/Y - Z%" progress
6. **Done**: All trimmed videos saved in the output directory

### Time Format

Timestamps can be in two formats:
- **HH:MM:SS.mmm**: e.g., `00:01:30.500` (1 minute, 30.5 seconds)
- **Decimal seconds**: e.g., `90.5` (90.5 seconds)

### Output Naming

By default, output files are named: `{original}_trimmed_{timestamp}.mp4`

Example: `myvideo_trimmed_20251019_143022.mp4`

## Configuration

Configuration is stored in:
- **Linux**: `~/.config/trimora/config.json`
- **macOS**: `~/Library/Application Support/Trimora/config.json`

### Example Config

```json
{
  "ffmpeg_path": "/usr/bin/ffmpeg",
  "output_directory": "~/Videos/Trimmed",
  "output_naming_pattern": "{name}_trimmed_{timestamp}",
  "recent_files_count": 5,
  "auto_open_output": false,
  "log_level": "info",
  "theme": "dark"
}
```

## Architecture

```
┌─────────────────────┐
│  GUI Layer (ImGui)  │  ← Main Thread
└──────────┬──────────┘
           │ (thread-safe queue)
           ▼
┌─────────────────────┐
│  Threading Layer    │
└──────────┬──────────┘
           ▼
┌─────────────────────┐
│  FFmpeg Executor    │  ← Worker Thread
└──────────┬──────────┘
           ▼
┌─────────────────────┐
│  Validation & Files │
└─────────────────────┘
```

## Development

### Project Structure

```
trimora/
├── src/
│   ├── main.cpp              # Entry point
│   ├── ffmpeg_executor.*     # FFmpeg process management
│   ├── validator.*           # Input validation
│   ├── file_manager.*        # File operations
│   ├── config_manager.*      # Configuration
│   └── gui/
│       ├── application.*     # Application lifecycle
│       └── main_window.*     # Main UI
├── external/
│   └── imgui/                # ImGui (cloned during setup)
├── tests/                    # Unit tests (TODO)
├── CMakeLists.txt            # Build configuration
└── README.md                 # This file
```

### Security Features

- **No `system()` calls**: Uses `boost::process` for secure process spawning
- **Path sanitization**: Validates and escapes all file paths
- **Input validation**: Rejects dangerous characters and formats
- **Permission checks**: Verifies read/write access before operations

## Troubleshooting

### FFmpeg Not Found

If you see "FFmpeg not detected":
1. Install FFmpeg: `sudo pacman -S ffmpeg` (Arch) or `sudo apt install ffmpeg` (Ubuntu)
2. Ensure it's in your PATH: `which ffmpeg`
3. Or configure custom path in `config.json`

### Build Errors

**ImGui not found:**
```bash
git clone --depth 1 https://github.com/ocornut/imgui.git external/imgui
```

**Boost not found:**
```bash
sudo pacman -S boost  # Arch
sudo apt install libboost-all-dev  # Ubuntu
```

**GLFW not found:**
```bash
sudo pacman -S glfw-x11  # Arch
sudo apt install libglfw3-dev  # Ubuntu
```

## Roadmap

- [x] Core trimming functionality
- [x] Input validation
- [x] Configuration management
- [x] Recent files
- [ ] File browser dialogs (native)
- [ ] Preview player
- [ ] Batch processing
- [ ] Timeline-based trimming
- [ ] Presets management
- [ ] GPU acceleration toggle
- [ ] Unit tests

## License

MIT License - See LICENSE file for details

## Contributing

Contributions welcome! Please ensure:
- Code follows C++20 standards
- All inputs are validated
- No use of `system()` calls
- Thread-safe UI updates
- Tests pass (when implemented)

## Credits

- Built with [Dear ImGui](https://github.com/ocornut/imgui)
- Powered by [FFmpeg](https://ffmpeg.org)
- Uses [Boost](https://www.boost.org) libraries

