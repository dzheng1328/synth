#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "param_queue.h"

static void counting_handler(const ParamMsg* change, void* userdata) {
    (void)change;
    int* counter = (int*)userdata;
    if (counter) {
        (*counter)++;
    }
}

int main(void) {
    printf("Running param_queue tests...\n");
    param_queue_init();

    ParamMsg change;
    memset(&change, 0, sizeof(change));

    // Test basic push/pop ordering
    assert(param_queue_dequeue(&change) == false && "Queue should be empty initially");

    ParamMsg master = {.id = PARAM_MASTER_VOLUME, .type = PARAM_FLOAT};
    master.value.f = 0.75f;
    assert(param_queue_enqueue(&master) == true);

    ParamMsg tempo = {.id = PARAM_TEMPO, .type = PARAM_FLOAT};
    tempo.value.f = 128.0f;
    assert(param_queue_enqueue(&tempo) == true);

    assert(param_queue_dequeue(&change) == true);
    assert(change.id == PARAM_MASTER_VOLUME);
    assert(change.type == PARAM_FLOAT);
    assert(fabsf(change.value.f - 0.75f) < 0.0001f);

    assert(param_queue_dequeue(&change) == true);
    assert(change.id == PARAM_TEMPO);
    assert(fabsf(change.value.f - 128.0f) < 0.0001f);

    // Test overflow handling
    for (int i = 0; i < PARAM_QUEUE_SIZE; ++i) {
    ParamMsg msg = {.id = PARAM_FX_DISTORTION_DRIVE, .type = PARAM_FLOAT};
        msg.value.f = (float)i;
        bool ok = param_queue_enqueue(&msg);
        assert(ok == true && "Queue should accept until full");
    }
    ParamMsg overflow = {.id = PARAM_FX_DISTORTION_DRIVE, .type = PARAM_FLOAT};
    overflow.value.f = 999.0f;
    assert(param_queue_enqueue(&overflow) == false &&
           "Queue should report full state");

    // Drain everything
    int drained = 0;
    while (param_queue_dequeue(&change) == true) {
        drained++;
    }
    assert(drained == PARAM_QUEUE_SIZE);

    // Stress-test repeated fill/drain cycles to emulate heavy UI automation bursts
    for (int round = 0; round < 8; ++round) {
        for (int i = 0; i < PARAM_QUEUE_SIZE / 2; ++i) {
            ParamMsg env = {.id = PARAM_ENV_ATTACK, .type = PARAM_FLOAT};
            env.value.f = (float)(round * 100 + i);
            assert(param_queue_enqueue(&env) == true);
        }
        int count = 0;
        param_queue_drain(counting_handler, &count);
        assert(count == PARAM_QUEUE_SIZE / 2);
        assert(param_queue_dequeue(&change) == false && "Queue should be empty after drain");
    }

    printf("param_queue tests passed.\n");
    return 0;
}
