cmake_minimum_required(VERSION 3.30)

project(Proj LANGUAGES CXX)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC settings
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # /MT[d]
  add_compile_options(
    $<$<CXX_COMPILER_ID:MSVC>:/MP>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    $<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
  )
  add_definitions(-DUNICODE -D_UNICODE)
endif()

# Output helper: put all outputs under GameDemo/<Config>
function(set_common_output_dirs tgt)
  set(root "${CMAKE_SOURCE_DIR}/GameDemo")
  foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    set(dir "${root}/${cfg}")
    set_target_properties(${tgt} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${cfg} "${dir}"
      ARCHIVE_OUTPUT_DIRECTORY_${cfg} "${dir}"
      LIBRARY_OUTPUT_DIRECTORY_${cfg} "${dir}"
      PDB_OUTPUT_DIRECTORY_${cfg}     "${dir}"
    )
  endforeach()
  set_target_properties(${tgt} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${root}/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${root}/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${root}/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY     "${root}/$<CONFIG>"
  )
  get_target_property(_type ${tgt} TYPE)
  if(MSVC AND _type STREQUAL "EXECUTABLE")
    set_property(TARGET ${tgt} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${root}/$(Configuration)")
  endif()
endfunction()

# Add dependencies in build order
add_subdirectory(GraphicsEngine)
add_subdirectory(PhysicsEngine)
add_subdirectory(Game)

# Ensure linking order (Game depends on both DLLs)
target_link_libraries(Game PRIVATE GraphicsEngine PhysicsEngine)

# Set Game as startup project for Visual Studio (AFTER all targets are defined)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Game)