#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DEVICE_IO
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#include "miniaudio.h"

#include "sample_io.h"

static int g_tests_run = 0;
static int g_tests_failed = 0;

static void report(bool pass, const char* name, const char* fmt, ...) {
    g_tests_run++;
    if (!pass) {
        g_tests_failed++;
    }

    va_list args;
    va_start(args, fmt);
    printf("[%s] %s - ", pass ? "PASS" : "FAIL", name);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

static void make_temp_path(char* out, size_t size, const char* tag) {
    if (!out || size == 0) {
        return;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    pid_t pid = getpid();
    snprintf(out, size, "/tmp/sio_%s_%ld_%ld.wav", tag,
             (long)pid, (long)ts.tv_nsec);
}

static bool write_roundtrip_test(void) {
    const uint32_t frames = 8192;
    const uint32_t channels = 2;
    const uint32_t sample_rate = 44100;
    float* source = malloc(sizeof(float) * frames * channels);
    if (!source) {
        report(false, "roundtrip", "out of memory allocating %u frames", frames);
        return false;
    }

    for (uint32_t i = 0; i < frames; i++) {
        float t = (float)i / (float)frames;
        float value = sinf(2.0f * (float)M_PI * 220.0f * t);
        source[i * channels + 0] = value;
        source[i * channels + 1] = -value;
    }

    char path[PATH_MAX];
    make_temp_path(path, sizeof(path), "roundtrip");

    bool wrote = sample_buffer_write_wav(path, source, frames, channels, sample_rate);
    if (!wrote) {
        report(false, "roundtrip", "failed to write temp wav %s", path);
        unlink(path);
        return false;
    }

    SampleBuffer buffer;
    sample_buffer_init(&buffer);
    bool loaded = sample_buffer_load_wav(&buffer, path);
    unlink(path);
    if (!loaded) {
        report(false, "roundtrip", "failed to reload %s", path);
        sample_buffer_free(&buffer);
        return false;
    }

    bool meta_ok = buffer.channels == channels &&
                   buffer.sample_rate == sample_rate &&
                   buffer.frame_count == frames;
    if (!meta_ok) {
        report(false, "roundtrip", "metadata mismatch (frames=%u/%u ch=%u/%u rate=%u/%u)",
              buffer.frame_count, frames, buffer.channels, channels,
              buffer.sample_rate, sample_rate);
        sample_buffer_free(&buffer);
        return false;
    }

    float max_error = 0.0f;
    for (uint32_t i = 0; i < frames * channels; i++) {
        float err = fabsf(buffer.data[i] - source[i]);
        if (err > max_error) {
            max_error = err;
        }
    }

    free(source);
    sample_buffer_free(&buffer);
    report(max_error < 1e-4f, "roundtrip", "max_error=%.6f", max_error);
    return max_error < 1e-4f;
}

static bool load_missing_file_test(void) {
    SampleBuffer buffer;
    sample_buffer_init(&buffer);
    bool loaded = sample_buffer_load_wav(&buffer, "/tmp/does_not_exist.wav");
    bool ok = !loaded && buffer.data == NULL && buffer.frame_count == 0;
    sample_buffer_free(&buffer);
    report(ok, "missing_file", "expected failure when loading missing file");
    return ok;
}

static bool invalid_write_args_test(void) {
    float dummy[2] = {0};
    bool ok = true;
    ok &= !sample_buffer_write_wav(NULL, dummy, 16, 1, 44100);
    ok &= !sample_buffer_write_wav("/tmp/invalid.wav", NULL, 16, 1, 44100);
    ok &= !sample_buffer_write_wav("/tmp/invalid.wav", dummy, 0, 1, 44100);
    report(ok, "invalid_write", "guard clauses rejected bad parameters");
    return ok;
}

int main(void) {
    printf("SAMPLE I/O REGRESSION TESTS\n");
    printf("==============================\n");

    write_roundtrip_test();
    load_missing_file_test();
    invalid_write_args_test();

    int passed = g_tests_run - g_tests_failed;
    printf("\nSummary: %d/%d tests passed\n", passed, g_tests_run);
    return g_tests_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
