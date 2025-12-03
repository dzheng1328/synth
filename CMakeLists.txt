cmake_minimum_required(VERSION 3.10)
project(synth_complete C)

include(FetchContent)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(USE_UI_ASSETS "Bundle UI assets" ON)

set(SYNTH_COMPLETE_SOURCES
    synth_complete.c
    synth_engine.c
    param_queue.c
    pa_ringbuffer.c
    nuklear_impl.c
    midi_input.c
    midi_shim.c
    preset.c
    project.c
    sample_io.c
    ui/style.c
    ui/draw_helpers.c
    ui/knob_custom.c
    third_party/glad/src/gl.c
    third_party/cjson/cJSON.c
)

add_executable(synth_complete_app ${SYNTH_COMPLETE_SOURCES})

target_include_directories(synth_complete_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.3.9
    FIND_PACKAGE_ARGS
)

FetchContent_MakeAvailable(glfw)

if(APPLE)
    target_link_libraries(synth_complete_app PRIVATE
        glfw
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        m
        pthread)
elseif(UNIX)
    target_link_libraries(synth_complete_app PRIVATE glfw m pthread dl)
elseif(WIN32)
    target_link_libraries(synth_complete_app PRIVATE glfw opengl32 gdi32 shell32 winmm)
endif()

if(USE_UI_ASSETS)
    set(UI_ASSET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui/assets)
    if(EXISTS ${UI_ASSET_DIR})
        add_custom_target(copy_ui_assets ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${UI_ASSET_DIR} ${CMAKE_CURRENT_BINARY_DIR}/assets)
        add_dependencies(synth_complete_app copy_ui_assets)
    endif()
endif()

install(TARGETS synth_complete_app DESTINATION bin)

message(STATUS "Synth Complete configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  System: ${CMAKE_SYSTEM_NAME}")
