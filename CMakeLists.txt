cmake_minimum_required(VERSION 3.10)
project(synth-c C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find math library (required for sin, cos, pow, etc.)
find_library(MATH_LIBRARY m)

# Source files
set(SOURCES
    main.c
)

# Executable
add_executable(synth ${SOURCES})

# Link libraries
if(MATH_LIBRARY)
    target_link_libraries(synth ${MATH_LIBRARY})
endif()

# Platform-specific audio libraries
if(UNIX AND NOT APPLE)
    # Linux: ALSA
    target_link_libraries(synth asound pthread)
elseif(APPLE)
    # macOS: Core Audio
    target_link_libraries(synth "-framework CoreAudio" "-framework AudioToolbox" pthread)
elseif(WIN32)
    # Windows: Windows Multimedia API
    target_link_libraries(synth winmm)
endif()

# Install target
install(TARGETS synth DESTINATION bin)

# Print configuration
message(STATUS "Synth-C Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "  System: ${CMAKE_SYSTEM_NAME}")
