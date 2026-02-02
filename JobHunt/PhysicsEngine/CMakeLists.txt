cmake_minimum_required(VERSION 3.30)

# Explicit header files list
set(PE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/Physics.h"
)

# Explicit source files list
set(PE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Physics.cpp"
)

add_library(PhysicsEngine SHARED ${PE_HEADERS} ${PE_SOURCES})

target_compile_features(PhysicsEngine PUBLIC cxx_std_20)
target_compile_definitions(PhysicsEngine
    PRIVATE
        _UNICODE
        UNICODE
        PHYSICSENGINE_EXPORTS
)
set_target_properties(PhysicsEngine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_include_directories(PhysicsEngine
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${PE_HEADERS} ${PE_SOURCES})

if (MSVC)
    target_compile_options(PhysicsEngine PRIVATE /W4 /permissive- /Zc:__cplusplus /EHsc /wd4251 /wd4275)
endif()

set_common_output_dirs(PhysicsEngine)