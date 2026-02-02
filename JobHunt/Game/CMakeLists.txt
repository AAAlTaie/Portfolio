cmake_minimum_required(VERSION 3.30)

# Explicit source files list (adjust based on your actual files)
set(GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
    # Add other .cpp files here as you create them
)

add_executable(Game WIN32 ${GAME_SOURCES})

target_compile_definitions(Game PRIVATE _UNICODE UNICODE)

# Link to libraries - CMake will handle include directories automatically
target_link_libraries(Game PRIVATE GraphicsEngine PhysicsEngine d3d12 dxgi d3dcompiler)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${GAME_SOURCES})

if (MSVC)
    target_compile_options(Game PRIVATE /W4 /permissive- /Zc:__cplusplus /EHsc /wd4251 /wd4275)
endif()

set_common_output_dirs(Game)

# Shaders: show but don't compile; copy after build
file(GLOB SHADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Assets/Shaders/*.hlsl"
)
if (SHADERS)
    target_sources(Game PRIVATE ${SHADERS})
    source_group(TREE "${CMAKE_SOURCE_DIR}/Assets/Shaders" PREFIX "Shaders" FILES ${SHADERS})
    set_source_files_properties(${SHADERS} PROPERTIES HEADER_FILE_ONLY ON)
    set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")
endif()

add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Game>/Shaders"
)
foreach(SH ${SHADERS})
    add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SH}" "$<TARGET_FILE_DIR:Game>/Shaders"
    )
endforeach()

add_dependencies(Game GraphicsEngine PhysicsEngine)