#!/bin/bash

# Unified build helper for the synth project.
#
# Usage examples:
#   ./build.sh                    # Build the minimal CLI demo via CMake (prompts to run)
#   ./build.sh synth_pro          # Compile + launch the full GUI with CoreMIDI + samples
#   ./build.sh synth_pro --no-run # Build GUI but leave it closed
#   ./build.sh synth_complete     # Build the legacy GUI stack

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

TARGET="cmake"
RUN_AFTER=1   # Used for GUI targets (cmake path still prompts)

usage() {
    cat <<'EOF'
Usage: ./build.sh [cmake|synth_pro|synth_complete] [--run|--no-run]

Targets:
  cmake           Build the original CLI demo via CMake (default)
  synth_pro       Build + run the modern GUI (GLFW + Nuklear + CoreMIDI)
  synth_complete  Build + run the legacy GUI stack

Flags:
  --run           Force auto-launch after building GUI targets (default)
  --no-run        Skip auto-launch (helpful for remote builds)
  -h, --help      Show this message

Tips:
  - Install GLFW via Homebrew: brew install glfw
  - Set GLFW_PREFIX manually if GLFW is in a non-standard location
EOF
}

while [[ $# -gt 0 ]]; do
    if [[ $1 == \#* ]]; then
        # Ignore everything after inline shell comments like "# builds ..."
        break
    fi
    case "$1" in
        cmake|synth_pro|synth_complete)
            TARGET="$1"
            shift
            ;;
        --run)
            RUN_AFTER=1
            shift
            ;;
        --no-run)
            RUN_AFTER=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage
            exit 1
            ;;
    esac
done

ensure_macos() {
    if [[ "$(uname -s)" != "Darwin" ]]; then
        echo "The GUI builds currently target macOS (Darwin) only." >&2
        exit 1
    fi
}

detect_glfw() {
    local prefix="${GLFW_PREFIX:-}"
    if [[ -z "$prefix" ]] && command -v brew >/dev/null 2>&1; then
        prefix="$(brew --prefix glfw 2>/dev/null || true)"
    fi

    GLFW_INCLUDE=()
    GLFW_LIBS=()

    if [[ -n "$prefix" && -d "$prefix" ]]; then
        GLFW_INCLUDE=(-I"$prefix/include")
        GLFW_LIBS=(-L"$prefix/lib" -lglfw)
        echo "Using GLFW from $prefix"
    else
        echo "Using system GLFW lookup (set GLFW_PREFIX to override)"
        GLFW_LIBS=(-lglfw)
    fi
}

build_with_cmake() {
    echo "================================================"
    echo "  Building Synth-C (CMake target: synth)"
    echo "================================================"
    echo ""

    if [[ ! -d build ]]; then
        echo "Creating build directory..."
        mkdir build
    fi

    pushd build >/dev/null
    echo "Running CMake..."
    cmake ..
    echo ""
    echo "Compiling..."
    make
    popd >/dev/null

    echo ""
    echo "================================================"
    echo "  Build Complete!"
    echo "================================================"
    echo "Run with: ./build/synth"
    echo ""

    if [[ $RUN_AFTER -eq 0 ]]; then
        echo "Skipping run prompt (--no-run supplied). Launch later with ./build/synth"
        return
    fi

    read -p "Run the CLI demo now? (y/n) " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Launching ./build/synth"
        echo ""
        ./build/synth
    else
        echo "Skipping run. Launch later with ./build/synth"
    fi
}

build_gui_target() {
    local name="$1"
    shift
    local output="$ROOT_DIR/${name}_app"
    local cc_bin="${CC:-clang}"
    local sources=("$@")

    ensure_macos
    detect_glfw

    local cflags=(-std=c11 -O2 -Wall -Wextra -Wpedantic -Wno-deprecated-declarations -DGL_SILENCE_DEPRECATION -I"$ROOT_DIR" -Ithird_party/cjson)
    cflags+=("${GLFW_INCLUDE[@]}")

    local frameworks=(
        -framework OpenGL
        -framework Cocoa
        -framework IOKit
        -framework CoreVideo
        -framework CoreAudio
        -framework AudioToolbox
        -framework CoreMIDI
        -framework CoreFoundation
    )

    echo "================================================"
    echo "  Building ${name} (GUI target)"
    echo "================================================"
    echo "Compiler : $cc_bin"
    echo "Output   : $output"
    echo "Sources  : ${sources[*]}"
    echo ""

    "$cc_bin" "${cflags[@]}" \
        "${sources[@]}" \
        "${GLFW_LIBS[@]}" \
        "${frameworks[@]}" \
        -lpthread -lm \
        -o "$output"

    echo "Built $output"
    if [[ $RUN_AFTER -eq 1 ]]; then
        echo "Launching $output..."
        "$output"
    else
        echo "Skipping auto-launch (pass --run to open automatically)."
    fi
}

case "$TARGET" in
    cmake)
        build_with_cmake
        ;;
    synth_pro)
        build_gui_target "synth_pro" \
            synth_pro.c \
            synth_engine.c \
            param_queue.c \
            pa_ringbuffer.c \
            nuklear_impl.c \
            midi_input.c \
            sample_io.c \
            preset.c \
            project.c \
            third_party/cjson/cJSON.c
        ;;
    synth_complete)
        build_gui_target "synth_complete" \
            synth_complete.c \
            synth_engine.c \
            param_queue.c \
            pa_ringbuffer.c \
            nuklear_impl.c \
            midi_input.c \
            midi_shim.c \
            ui/style.c \
            ui/draw_helpers.c \
            ui/knob_custom.c \
            preset.c \
            project.c \
            third_party/cjson/cJSON.c
        ;;
    *)
        echo "Unhandled target: $TARGET" >&2
        exit 1
        ;;
esac
