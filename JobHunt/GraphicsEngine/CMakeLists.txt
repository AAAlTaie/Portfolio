
cmake_minimum_required(VERSION 3.30)

# GraphicsEngine sources
file(GLOB_RECURSE GE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)
file(GLOB_RECURSE GE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
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
