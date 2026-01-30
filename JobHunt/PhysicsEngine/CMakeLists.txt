
cmake_minimum_required(VERSION 3.30)

# PhysicsEngine sources
file(GLOB_RECURSE PE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)
file(GLOB_RECURSE PE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
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
