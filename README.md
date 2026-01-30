# Portfolio:  DirectX12 3D Rendering Engine

## Project Overview
A modular DirectX12-based 3D rendering engine with advanced graphics features, built with C++20 and CMake.

## Architecture

### Project Structure
```
Proj/
├── Game/                    # Main executable
│   ├── src/main.cpp
│   ├── Shaders/
│   │   ├── Basic.hlsl      # Line/unlit shaders
│   │   └── BasicLit.hlsl   # Lit triangle shaders
│   └── CMakeLists.txt
├── GraphicsEngine/          # Core rendering DLL
│   ├── include/GraphicsEngine/
│   │   ├── Renderer.h      # Main renderer class
│   │   ├── Camera.h        # Camera system
│   │   ├── Geometry.h      # Geometry generation
│   │   ├── D3D12Helpers.h  # DX12 utilities
│   │   ├── SolMath.h       # Math library
│   │   └── Export.h        # DLL export macros
│   ├── src/GraphicsEngine/
│   │   └── Renderer.cpp    # Renderer implementation
│   └── CMakeLists.txt
├── PhysicsEngine/          # Physics simulation DLL
│   ├── include/PhysicsEngine/
│   │   └── Physics.h
│   ├── src/PhysicsEngine/
│   │   └── Physics.cpp
│   └── CMakeLists.txt
└── GameDemo/               # Build output directory
    ├── Debug/
    │   ├── Game.exe
    │   ├── GraphicsEngine.dll
    │   ├── PhysicsEngine.dll
    │   └── Shaders/
    └── Release/
```

### Build System
- **CMake 3.30+** with C++20 standard
- **MSVC runtime**: MultiThreaded (/MT) configuration
- **Output organization**: All binaries centralized in `GameDemo/<Config>/`
- **Dependencies**: Game depends on GraphicsEngine and PhysicsEngine DLLs

## Core Rendering System

### Initialization Pipeline
1. **Device Creation**: DX12 device with debug layer support (debug builds)
2. **Command Objects**: Queue, allocators, command list, fence
3. **Swapchain**: Triple-buffered flip-discard swapchain
4. **Depth Buffer**: Standard Z-buffer (D32_FLOAT)
5. **Root Signature & PSOs**: Pre-compiled shader pipelines
6. **Geometry Buffers**: Static vertex buffers for lines and triangles
7. **Shadow Map**: 2048×2048 directional shadow map

### Render Pipeline (Frame)
```cpp
1. RenderShadowPass()     // Depth-only from light perspective
2. Transition to RENDER_TARGET
3. RecordDrawCalls()      // World rendering with frustum culling
4. RenderHUD()           // Screen-space UI
5. Transition to PRESENT
6. Present with VSync control
```

## Key Features

### Lighting & Shadows
- **Directional Light**: Manual (J/L/I/K) or auto-orbit mode
- **Shadow Mapping**: 2048×2048 depth texture with hardware PCF
- **Light Controls**: Toggle (H), auto-orbit (N), intensity adjustments
- **Shadow Quality**: Orthographic projection with adjustable bounds

### Camera System
- **Three Modes**:
  - Free camera (WASD + mouse)
  - Third-person (follows player)
  - Orbit (around focus point)
- **Controls**: FOV zoom, sensitivity/acceleration tuning
- **Smoothing**: Mouse acceleration with configurable parameters

### Geometry & Rendering
- **Static Geometry**:
  - Grid (100×100 units, toggle with G)
  - World axes (RGB coordinate indicators)
  - Test cube (transformable, toggle with T)
- **Dynamic Geometry**:
  - 200 random colored cubes (toggle with R)
  - Player frustum visualization (toggle with F)
  - HUD elements (FPS, position, orientation)
- **Vertex Formats**:
  - `VertexPC`: Position + Color (lines)
  - `VertexPNC`: Position + Normal + Color (lit triangles)

### Frustum Culling
- **Visual Debug**: Full frustum visualization with plane normals
- **Offset Controls**: Adjust frustum position relative to player
- **Culling Planes**: Configurable near/far planes (O, +, -, 9, 0)
- **Algorithm**: AABB vs. 6-plane intersection test
- **Performance**: Culls 200 random cubes based on player frustum

### HUD System
- **Crosshair**: Centered screen reticle
- **FPS Display**: 7-segment digital readout
- **Position/Orientation**: Camera world coordinates, yaw/pitch
- **Speed Indicator**: Meters per second estimation
- **Toggle Status**: Visual indicators for all enabled features
- **Pixel-Perfect**: Orthographic projection for screen-space rendering

## Resource Management

### Memory Architecture
- **Constant Buffer Ring**: 256KB upload heap with 256-byte alignment
- **Descriptor Heaps**:
  - RTV heap (3 back buffers)
  - DSV heap (main depth)
  - Shadow DSV heap
  - SRV heap (shadow map + future textures)
- **Transient Resources**: Upload buffers for dynamic geometry (HUD, frustum lines)
- **Static Buffers**: Pre-uploaded vertex buffers for grid, axes, cube

### Pipeline State Objects
1. **m_pso**: Lit triangles (depth test on, two-sided)
2. **m_psoLines**: Unlit lines (no depth write, GS thickness)
3. **m_psoNoDepth**: HUD triangles (depth test off)
4. **m_psoShadow**: Depth-only shadow pass

### Root Signature Layout
```
b0: SceneCB (160 bytes)
    - MVP matrix (64)
    - Light direction (12)
    - Viewport dimensions (8)
    - Line thickness (4)
    - Light VP matrix (64)
t0: ShadowMap SRV
s0: Linear clamp sampler
s1: Comparison sampler (PCF)
```

## Input System

### Keyboard Controls
| Key | Function | Category |
|-----|----------|----------|
| WASD/QE | Camera movement | Navigation |
| Arrow Keys | Player movement | Player |
| RMB + Drag | Camera look | Camera |
| Mouse Wheel | FOV adjustment | Camera |
| G | Toggle grid | Visual |
| F | Toggle frustum | Debug |
| V | Toggle VSync | Performance |
| H | Toggle lighting | Lighting |
| B | Toggle shadows | Lighting |
| T | Toggle test cube | Visual |
| R | Toggle random cubes | Visual |
| N | Toggle light auto-orbit | Lighting |
| C | Cycle camera modes | Camera |
| O | Toggle culling override | Debug |

### Advanced Controls
- **[ ]**: Adjust mouse sensitivity
- **; '**: Adjust mouse acceleration
- **+ -**: Adjust far culling plane
- **9 0**: Adjust near culling plane
- **IJKL**: Manual light direction
- **Home/End/Insert/Delete/M**: Frustum offset adjustments
- **Backspace**: Reset frustum offset

## Performance Features

### Optimization
- **Triple Buffering**: 3-frame flight for CPU/GPU parallelism
- **VSync Control**: Toggle for performance testing
- **Frustum Culling**: Reduces draw calls for occluded objects
- **Upload Management**: Reusable constant buffer ring
- **Resource Barriers**: Minimal state transitions
- **Descriptor Reuse**: Static samplers, shared SRV heap

### Monitoring
- **FPS Display**: Real-time frames per second
- **Frame Time Graph**: 160-frame history buffer
- **Title Bar Stats**: Comprehensive system status
- **Memory Tracking**: Transient resource cleanup per frame

## Shader System

### HLSL Structure
- **Basic.hlsl**: Line rendering (VS/PS, optional GS for thickness)
- **BasicLit.hlsl**: Lit triangle rendering (VS/PS with normals)
- **Compilation**: Runtime compilation with debug symbols
- **Error Reporting**: Output window feedback for compile errors

### Constant Buffer (SceneCB)
```cpp
struct SceneCB {
    float4x4 mvp;
    float3 lightDir;
    float pad0;
    float2 viewport;
    float thicknessPx;
    float pad1;
    float4x4 lightVP;
};
```

## Development Features

### Debug Visualization
- **Frustum Planes**: Colored normals for each clipping plane
- **Culling Feedback**: Random cubes disappear when outside frustum
- **Player Indicator**: RGB axes at player position
- **Light Direction**: Visualized through shadow projection
- **Grid Toggle**: Reference plane for orientation

### Build Configuration
- **Debug Builds**: DX12 debug layer, shader debug symbols
- **Release Builds**: Optimized, no debug overhead
- **Output Management**: All binaries in single directory
- **PDB Files**: Separate debug symbols for crash analysis

## Extensibility

### Module Design
- **GraphicsEngine.dll**: Self-contained rendering module
- **Clean Interfaces**: Export.h defines public API
- **DLL Boundaries**: Proper symbol exporting/importing
- **Dependency Order**: CMake ensures correct build sequence

### Adding New Features
1. **New Geometry**: Add to CreateGeometry() method
2. **New Shaders**: Create HLSL file, add PSO in CreateRootAndPSO()
3. **New Render Pass**: Add to Render() method with proper barriers
4. **New HUD Elements**: Extend RenderHUD() method
5. **New Controls**: Add to OnKeyDown() with visual feedback

## Technical Specifications

### DirectX12 Requirements
- **Feature Level**: 12.0
- **Heap Types**: Default, Upload, Readback
- **Descriptor Heaps**: RTV, DSV, CBV/SRV/UAV
- **Resource States**: Proper barriers for all transitions
- **Fence Synchronization**: GPU/CPU synchronization points

### System Requirements
- **Windows 10/11**: DX12 compatible OS
- **Visual Studio**: 2019+ with C++20 support
- **Graphics Card**: DX12 compatible GPU
- **Memory**: 2GB+ VRAM recommended

## Known Architecture

### Strengths
- **Modular Design**: Clean separation between graphics, physics, game logic
- **Production Ready**: Proper error handling, resource management
- **Debug Friendly**: Extensive visualization tools
- **Performance Conscious**: Culling, batching, efficient barriers
- **User Configurable**: Many runtime-adjustable parameters

### Current Limitations
- **Single Shadow Map**: One directional light only
- **Basic Geometry**: Primitive-based rendering (cubes, lines, grid)
- **No Texture Support**: Color-only materials
- **Fixed Pipeline**: No material system or deferred rendering

## Getting Started

### Building
```bash
mkdir build
cd build
cmake .. -A x64
cmake --build . --config Debug
```

### Running
- Execute `GameDemo/Debug/Game.exe`
- Use mouse + keyboard controls
- Refer to title bar for active features
- Press F1 (if implemented) for control reference

### Development
1. Modify shaders in `Game/Shaders/`
2. Update geometry in `Renderer::CreateGeometry()`
3. Add new features to `Renderer` class
4. Rebuild GraphicsEngine.dll
5. Test with Game.exe

## Future Roadmap (Potential Extensions)
- **Material System**: Textures, PBR workflows
- **Deferred Rendering**: Multiple G-buffers
- **Multiple Lights**: Point, spot, area lights
- **Particle System**: GPU-based particle effects
- **Post-Processing**: Bloom, tonemapping, FXAA
- **Model Loading**: OBJ/FBX import support
- **Scene Graph**: Hierarchical transformation system

---

## This engine serves as a solid foundation for DirectX12 rendering with production-quality architecture, suitable for educational use or as a starting point for game development projects.