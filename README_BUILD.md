# DirectX12 3D Engine - Build Instructions

## Prerequisites
- **Windows 10/11** (DirectX12 requires Windows)
- **Visual Studio 2022** (Community or higher) with:
  - Desktop development with C++
  - Windows 10/11 SDK
- **CMake 3.30+** (Get from [cmake.org](https://cmake.org/download/))
- **Git** (for cloning)

## Quick Build (Command Line)
```bash
# Clone the repository
git clone https://github.com/yourusername/dx12-engine.git
cd dx12-engine

# Configure with CMake
cmake -B build -A x64

# Build
cmake --build build --config Debug

# Run (from build directory)
cd build/GameDemo/Debug
Game.exe