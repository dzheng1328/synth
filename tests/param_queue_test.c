#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "param_queue.h"

static void counting_handler(const ParamChange* change, void* userdata) {
    (void)change;
    int* counter = (int*)userdata;
    if (counter) {
        (*counter)++;
    }
}

int main(void) {
    printf("Running param_queue tests...\n");
    param_queue_init();

    ParamChange change;

    // Test basic push/pop ordering
    assert(param_queue_pop(&change) == 0 && "Queue should be empty initially");

    assert(param_queue_push(PARAM_MASTER_VOLUME, 0.75f) == 1);
    assert(param_queue_push(PARAM_TEMPO, 128.0f) == 1);

    assert(param_queue_pop(&change) == 1);
    assert(change.type == PARAM_MASTER_VOLUME);
    assert(fabsf(change.value - 0.75f) < 0.0001f);

    assert(param_queue_pop(&change) == 1);
    assert(change.type == PARAM_TEMPO);
    assert(fabsf(change.value - 128.0f) < 0.0001f);

    // Test overflow handling
    for (int i = 0; i < PARAM_QUEUE_SIZE; ++i) {
        int ok = param_queue_push(PARAM_FX_DISTORTION_DRIVE, (float)i);
        assert(ok == 1 && "Queue should accept until full");
    }
    assert(param_queue_push(PARAM_FX_DISTORTION_DRIVE, 999.0f) == 0 &&
           "Queue should report full state");

    // Drain everything
    int drained = 0;
    while (param_queue_pop(&change) == 1) {
        drained++;
    }
    assert(drained == PARAM_QUEUE_SIZE);

    // Stress-test repeated fill/drain cycles to emulate heavy UI automation bursts
    for (int round = 0; round < 8; ++round) {
        for (int i = 0; i < PARAM_QUEUE_SIZE / 2; ++i) {
            float value = (float)(round * 100 + i);
            assert(param_queue_push(PARAM_ENV_ATTACK, value) == 1);
        }
        int count = 0;
        param_queue_drain(counting_handler, &count);
        assert(count == PARAM_QUEUE_SIZE / 2);
        assert(param_queue_pop(&change) == 0 && "Queue should be empty after drain");
    }

    printf("param_queue tests passed.\n");
    return 0;
}
