cmake_minimum_required(VERSION 3.16)
project(rt-stt VERSION 1.0.0 LANGUAGES CXX C)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags for Apple Silicon
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -mtune=native")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -march=native -mtune=native")
endif()

# Find required packages
find_package(Threads REQUIRED)

# Include directories - Set up proper include paths
set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/whisper.cpp/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/whisper.cpp/ggml/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog/include
)

include_directories(${INCLUDE_DIRS})

# Also add src subdirectories to allow includes like "stt/engine.h"
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Find whisper library
set(WHISPER_BUILD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/whisper.cpp/build)
set(WHISPER_LIB_PATH ${WHISPER_BUILD_DIR}/src/libwhisper.dylib)
set(GGML_LIB_PATH ${WHISPER_BUILD_DIR}/ggml/src/libggml.dylib)
set(GGML_METAL_LIB_PATH ${WHISPER_BUILD_DIR}/ggml/src/ggml-metal/libggml-metal.dylib)
set(GGML_CPU_LIB_PATH ${WHISPER_BUILD_DIR}/ggml/src/libggml-cpu.dylib)
set(GGML_BLAS_LIB_PATH ${WHISPER_BUILD_DIR}/ggml/src/ggml-blas/libggml-blas.dylib)

if(NOT EXISTS ${WHISPER_LIB_PATH})
    message(FATAL_ERROR "whisper.cpp library not found. Please run ./scripts/download_dependencies.sh first")
endif()

# Add whisper and ggml libraries
add_library(whisper SHARED IMPORTED)
set_target_properties(whisper PROPERTIES IMPORTED_LOCATION ${WHISPER_LIB_PATH})

add_library(ggml SHARED IMPORTED)
set_target_properties(ggml PROPERTIES IMPORTED_LOCATION ${GGML_LIB_PATH})

add_library(ggml-metal SHARED IMPORTED)
set_target_properties(ggml-metal PROPERTIES IMPORTED_LOCATION ${GGML_METAL_LIB_PATH})

add_library(ggml-cpu SHARED IMPORTED)
set_target_properties(ggml-cpu PROPERTIES IMPORTED_LOCATION ${GGML_CPU_LIB_PATH})

add_library(ggml-blas SHARED IMPORTED)
set_target_properties(ggml-blas PROPERTIES IMPORTED_LOCATION ${GGML_BLAS_LIB_PATH})

# Source files
set(SOURCES
    src/main.cpp
    src/stt/engine.cpp
    src/stt/whisper_wrapper.cpp
    src/audio/capture.cpp
    src/audio/vad.cpp
    src/ipc/server.cpp
    src/config/config.cpp
    src/utils/terminal_output.cpp
)

# Test source files (without main.cpp)
set(TEST_SOURCES
    src/test_main.cpp
    src/stt/engine.cpp
    src/stt/whisper_wrapper.cpp
    src/audio/capture.cpp
    src/audio/vad.cpp
    src/config/config.cpp
    src/ipc/server.cpp
    src/utils/terminal_output.cpp
)

# Main executable
add_executable(rt-stt ${SOURCES})

# Link libraries
target_link_libraries(rt-stt
    whisper
    ggml
    ggml-metal
    ggml-cpu
    ggml-blas
    ${CMAKE_THREAD_LIBS_INIT}
)

# macOS specific frameworks
if(APPLE)
    target_link_libraries(rt-stt
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
        "-framework Accelerate"
        "-framework Metal"
        "-framework MetalKit"
    )
endif()

# Test executable for real-time validation
add_executable(rt-stt-test
    src/test_main.cpp
    src/stt/engine.cpp
    src/stt/whisper_wrapper.cpp
    src/audio/capture.cpp
    src/audio/vad.cpp
    src/utils/terminal_output.cpp
    src/config/config.cpp
)

target_link_libraries(rt-stt-test
    whisper
    ggml
    ggml-metal
    ggml-cpu
    ggml-blas
    ${CMAKE_THREAD_LIBS_INIT}
)

if(APPLE)
    target_link_libraries(rt-stt-test
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
        "-framework Accelerate"
        "-framework Metal"
        "-framework MetalKit"
    )
endif()

# Installation
install(TARGETS rt-stt DESTINATION bin)
install(DIRECTORY scripts/ DESTINATION /Library/LaunchDaemons
        FILES_MATCHING PATTERN "*.plist")
install(DIRECTORY config/ DESTINATION /usr/local/etc/rt-stt
        FILES_MATCHING PATTERN "*.json")
