cmake_minimum_required(VERSION 3.10)
project(synth_complete C)

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
    third_party/cjson/cJSON.c
)

add_executable(synth_complete_app ${SYNTH_COMPLETE_SOURCES})

target_include_directories(synth_complete_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(APPLE)
    target_link_libraries(synth_complete_app
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        glfw
        m
        pthread)
elseif(UNIX)
    target_link_libraries(synth_complete_app m pthread)
elseif(WIN32)
    target_link_libraries(synth_complete_app winmm)
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
