#ifndef SAMPLE_IO_H
#define SAMPLE_IO_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    float* data;
    uint32_t frame_count;
    uint32_t channels;
    uint32_t sample_rate;
} SampleBuffer;

void sample_buffer_init(SampleBuffer* buffer);
void sample_buffer_free(SampleBuffer* buffer);
bool sample_buffer_load_wav(SampleBuffer* buffer, const char* path);
bool sample_buffer_write_wav(const char* path, const float* interleaved,
                             uint32_t frame_count, uint32_t channels,
                             uint32_t sample_rate);

#endif // SAMPLE_IO_H
