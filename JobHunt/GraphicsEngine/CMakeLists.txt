cmake_minimum_required(VERSION 3.30)

# Explicit header files list
set(GE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Camera.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/D3D12Helpers.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Memory/DescriptorRing.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Memory/DescriptorSystem.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Export.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/FrameResources.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Geometry.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/GraphicsEngine.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Memory/LinearArena.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Renderer.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/SolMath.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Memory/UploadAlloc.h"
)

# Explicit source files list  
set(GE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Camera.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Geometry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/GraphicsEngine.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Renderer.cpp"
)

add_library(GraphicsEngine SHARED ${GE_HEADERS} ${GE_SOURCES})

target_compile_features(GraphicsEngine PUBLIC cxx_std_20)
target_compile_definitions(GraphicsEngine
    PRIVATE
        _UNICODE
        UNICODE
        GRAPHICSENGINE_EXPORTS
)

target_include_directories(GraphicsEngine
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(GraphicsEngine
    PUBLIC
        d3d12
        dxgi
        d3dcompiler
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${GE_HEADERS} ${GE_SOURCES})

if (MSVC)
    target_compile_options(GraphicsEngine PRIVATE /W4 /permissive- /Zc:__cplusplus /EHsc /wd4251 /wd4275)
endif()

set_common_output_dirs(GraphicsEngine)