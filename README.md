# CHEngine

A cross-platform game engine written in C++ with support for Windows, macOS, and Linux.

## Features

- **Cross-platform support**: Build and run on Windows (MSVC), macOS (Clang), and Linux (GCC/Clang)
- **OpenGL rendering**: Hardware-accelerated graphics with GLAD and GLFW
- **Modular architecture**: Pluggable renderer modules with dynamic loading
- **Event system**: Event-driven application with layer-based rendering pipeline
- **Logging**: Structured logging with spdlog

## Supported Platforms

| Platform | Compiler | Status |
|----------|----------|--------|
| Windows  | MSVC 2022 | ✅ Supported |
| macOS    | Clang    | ✅ Supported |
| Linux    | GCC/Clang| ✅ Supported |

## Project Structure

```
CHEngine/
├── Core/                          # Core library
│   ├── src/
│   │   ├── Core.h                # Platform-specific macros
│   │   ├── Log/                  # Logging system
│   │   └── Memory/               # Memory utilities
│   └── vendor/spdlog/            # Logging dependency
├── CHEngine/                       # Main engine
│   ├── src/
│   │   ├── CHEngine/             # Application, events, layers
│   │   ├── Platform/Desktop/     # Cross-platform window implementation
│   │   └── Interfaces/           # Renderer interfaces
│   ├── vendor/
│   │   ├── GLFW/                 # Window management
│   │   └── GLM/                  # Math library
│   └── CMakeLists.txt
├── Modules/
│   └── Rendering/RendererOGL/    # OpenGL renderer module
├── Sandbox/                        # Example application
└── CMakeLists.txt                 # Root CMake configuration
```

## Dependencies

### Required
- **CMake** 3.20+
- **C++20** compiler (MSVC 2022, Clang, or GCC 10+)

### Included (Vendored)
- **spdlog** - Fast C++ logging library
- **GLFW** - Window and input management
- **GLAD** - OpenGL loader
- **GLM** - Mathematics library

## Building

### Windows (MSVC)

```bash
mkdir build
cd build
cmake -G "Visual Studio 17 2022" ..
cmake --build . --config Debug
```

### macOS (Clang)

```bash
mkdir build
cd build
cmake -G "Xcode" ..
# or
cmake -G "Unix Makefiles" ..
cmake --build .
```

### Linux (GCC/Clang)

```bash
mkdir build
cd build
cmake -G "Unix Makefiles" ..
cmake --build .
```

## Configuration

The project uses CMake with the following options:

```cmake
-DCHE_BUILD_SANDBOX=ON      # Build example Sandbox application
-DCHE_BUILD_OPENGL=ON       # Build OpenGL renderer module
```

## Platform Defines

The engine automatically defines platform-specific macros:

- `CHE_PLATFORM_WINDOWS` - Windows platform
- `CHE_PLATFORM_APPLE` - macOS platform
- `CHE_PLATFORM_LINUX` - Linux platform
- `CHE_PLATFORM_UNIX` - Any Unix-like system (macOS or Linux)

## Architecture Highlights

### Cross-Platform C++ Macros

```cpp
// Core.h provides platform-agnostic export macros
#define CHENGINE_API __attribute__((visibility("default")))  // Unix
#define CHENGINE_API __declspec(dllexport)                   // Windows

// Debugbreak macro
#define CHE_DEBUGBREAK() __builtin_trap()      // GCC/Clang
#define CHE_DEBUGBREAK() __debugbreak()        // MSVC
```

### Compiler Flags

**MSVC:**
```cmake
/utf-8 /W4              # Warnings
/Od /Zi                 # Debug
/O2                     # Release
```

**GCC/Clang:**
```cmake
-Wall -Wextra -Wpedantic    # Warnings
-fvisibility=hidden         # Symbol visibility
-g -O0                      # Debug
-O2                         # Release
```

### Dynamic Module Loading

Platform-specific module names are handled transparently:

```cpp
#if defined(CHE_PLATFORM_WINDOWS)
    LoadModule("RendererOGL.dll");
#elif defined(CHE_PLATFORM_APPLE)
    LoadModule("libRendererOGL.dylib");
#else
    LoadModule("libRendererOGL.so");
#endif
```

## Compilation Outputs

Build artifacts are organized by platform and configuration:

```
bin/
├── Debug-windows-x64/
├── Debug-macos-x64/
├── Debug-linux-x64/
├── Release-windows-x64/
└── ...
```

## Running the Example

After building, run the Sandbox application:

```bash
# Windows
./bin/Debug-windows-x64/Sandbox/Sandbox.exe

# macOS
./bin/Debug-macos-x64/Sandbox/Sandbox

# Linux
./bin/Debug-linux-x64/Sandbox/Sandbox
```

## Development

### Adding Platform-Specific Code

Use the platform defines in conditional compilation:

```cpp
#ifdef CHE_PLATFORM_WINDOWS
    // Windows-specific code
#elif defined(CHE_PLATFORM_UNIX)
    // Unix-specific code (Linux/macOS)
#endif
```

### Compiler-Specific Code

For compiler differences, use:

```cpp
#ifdef _MSC_VER
    // MSVC-specific
#else
    // GCC/Clang
#endif
```

## Key Changes (v1.0 - Cross-Platform)

- Removed Windows-only guards from CMakeLists.txt
- Added platform detection for APPLE, LINUX, UNIX
- Replaced `__declspec(dllexport/import)` with `__attribute__((visibility()))`
- Unified compiler flags (MSVC vs GCC/Clang)
- Renamed `WindowsWindow` → `DesktopWindow` (GLFW-based, platform-agnostic)
- Platform-specific module loading (.dll/.so/.dylib)
- Fixed dlopen/dlsym bugs on non-Windows platforms

## License

See LICENSE file for details.

## Contributing

Contributions are welcome! Please ensure:
- Code compiles on Windows, macOS, and Linux
- Platform-specific code is properly guarded with defines
- CMakeLists.txt changes maintain cross-platform compatibility
