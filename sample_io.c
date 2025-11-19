#include "sample_io.h"
#include "miniaudio.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

void sample_buffer_init(SampleBuffer* buffer) {
    if (!buffer) {
        return;
    }
    buffer->data = NULL;
    buffer->frame_count = 0;
    buffer->channels = 0;
    buffer->sample_rate = 0;
}

void sample_buffer_free(SampleBuffer* buffer) {
    if (!buffer) {
        return;
    }
    free(buffer->data);
    buffer->data = NULL;
    buffer->frame_count = 0;
    buffer->channels = 0;
    buffer->sample_rate = 0;
}

bool sample_buffer_load_wav(SampleBuffer* buffer, const char* path) {
    if (!buffer || !path || path[0] == '\0') {
        return false;
    }

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder decoder;
    ma_result result = ma_decoder_init_file(path, &config, &decoder);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "❌ Failed to open sample '%s' (error %d)\n", path, result);
        return false;
    }

    ma_uint64 file_frames = 0;
    result = ma_decoder_get_length_in_pcm_frames(&decoder, &file_frames);
    if (result != MA_SUCCESS || file_frames == 0) {
        ma_decoder_uninit(&decoder);
        fprintf(stderr, "❌ Unable to determine sample length for '%s'\n", path);
        return false;
    }

    if (file_frames > UINT32_MAX) {
        file_frames = UINT32_MAX;
    }

    ma_uint32 channels = decoder.outputChannels;
    if (channels == 0) {
        channels = config.channels;
    }
    if (channels == 0) {
        channels = 1;
    }

    ma_uint32 sample_rate = decoder.outputSampleRate;
    if (sample_rate == 0) {
        sample_rate = config.sampleRate;
    }
    if (sample_rate == 0) {
        sample_rate = 44100;
    }

    float* data = (float*)malloc((size_t)file_frames * channels * sizeof(float));
    if (!data) {
        ma_decoder_uninit(&decoder);
        fprintf(stderr, "❌ Out of memory loading sample '%s'\n", path);
        return false;
    }

    ma_uint64 total_read = 0;
    while (total_read < file_frames) {
        ma_uint64 frames_to_read = file_frames - total_read;
        ma_uint64 read = 0;
        ma_result read_result = ma_decoder_read_pcm_frames(&decoder,
                                                           data + (total_read * channels),
                                                           frames_to_read,
                                                           &read);
        if (read_result != MA_SUCCESS || read == 0) {
            break;
        }
        total_read += read;
    }
    ma_decoder_uninit(&decoder);

    if (total_read == 0) {
        free(data);
        fprintf(stderr, "❌ No audio frames read from '%s'\n", path);
        return false;
    }

    sample_buffer_free(buffer);
    buffer->data = data;
    buffer->frame_count = (uint32_t)total_read;
    buffer->channels = channels;
    buffer->sample_rate = sample_rate;

    return true;
}

bool sample_buffer_write_wav(const char* path, const float* interleaved,
                             uint32_t frame_count, uint32_t channels,
                             uint32_t sample_rate) {
    if (!path || !interleaved || frame_count == 0 || channels == 0 || sample_rate == 0) {
        return false;
    }

    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav,
                                                      ma_format_f32,
                                                      channels,
                                                      sample_rate);
    ma_encoder encoder;
    ma_result result = ma_encoder_init_file(path, &config, &encoder);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "❌ Failed to open export '%s' (error %d)\n", path, result);
        return false;
    }

    ma_uint64 written = 0;
    ma_result write_result = ma_encoder_write_pcm_frames(&encoder,
                                                         interleaved,
                                                         frame_count,
                                                         &written);
    ma_encoder_uninit(&encoder);

    if (write_result != MA_SUCCESS || written != frame_count) {
        fprintf(stderr, "⚠️ Only wrote %llu/%u frames to '%s'\n",
                (unsigned long long)written, frame_count, path);
        return false;
    }

    return true;
}
