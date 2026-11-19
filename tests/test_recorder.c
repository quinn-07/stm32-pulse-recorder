#include <assert.h>
#include <stdio.h>
#include "recorder.h"

static void trigger_and_finish(Recorder *r, uint32_t *t, uint32_t period)
{
    recorder_edge(r, *t += period);
    assert(r->state == COLLECTING);
    for (unsigned i = 1; i <= POST_PERIODS; ++i) {
        recorder_edge(r, *t += 1200u + i); /* a further anomaly must not retrigger */
        assert(r->post_count == i);
        assert(r->state == (i == POST_PERIODS ? FROZEN : COLLECTING));
    }
}

static void check_history_and_thresholds(void)
{
    Recorder r;
    uint32_t t = 0;
    recorder_arm(&r);
    recorder_edge(&r, t);
    assert(r.count == 0 && r.state == ARMED);
    for (unsigned i = 0; i <= 200; ++i)
        recorder_edge(&r, t += 900u + i);
    assert(r.count == PRE_PERIODS && r.state == ARMED);
    trigger_and_finish(&r, &t, 1101);
    assert(r.count == WINDOW_SIZE && r.pre_count == PRE_PERIODS);
    for (unsigned i = 0; i < PRE_PERIODS; ++i)
        assert(recorder_period(&r, i) == 1100u - (PRE_PERIODS - 1) + i);
    assert(recorder_period(&r, r.pre_count) == 1101);
    for (unsigned i = 1; i <= POST_PERIODS; ++i)
        assert(recorder_period(&r, r.pre_count + i) == 1200u + i);
    unsigned next = r.next;
    recorder_edge(&r, t + 1);
    recorder_lost(&r);
    assert(r.state == FROZEN && r.count == WINDOW_SIZE && r.next == next);
}

static void check_rearm_and_early_trigger(void)
{
    Recorder r;
    uint32_t t = 0;
    recorder_arm(&r);
    recorder_edge(&r, t);
    recorder_edge(&r, t += 1000);
    recorder_lost(&r);
    recorder_edge(&r, t += 1500);
    assert(r.state == CAPTURE_LOST && r.count == 1);

    recorder_arm(&r);
    recorder_edge(&r, t += 900000u);
    assert(r.state == ARMED && r.count == 0);
    trigger_and_finish(&r, &t, 899);
    assert(r.pre_count == 0 && r.count == 1 + POST_PERIODS);
    assert(recorder_period(&r, 0) == 899);

    recorder_arm(&r);
    recorder_edge(&r, 0);
    recorder_edge(&r, 1500);
    recorder_lost(&r);
    recorder_edge(&r, 2500);
    assert(r.state == CAPTURE_LOST && r.post_count == 0);
}

static void check_timer_rollover(void)
{
    /* Both flags pending: edge immediately before vs after counter wrap. */
    assert(capture_timestamp(0x10000, 0xfffe, true) == 0x1fffe);
    assert(capture_timestamp(0x10000, 0x0002, true) == 0x20002);
    assert(capture_timestamp(0x20000, 0x0002, false) == 0x20002);

    Recorder r;
    recorder_arm(&r);
    recorder_edge(&r, 0xfffffe00u);
    recorder_edge(&r, capture_timestamp(0xffff0000u, 0x01e8, true));
    assert(r.state == ARMED && recorder_period(&r, 0) == 1000);
    recorder_edge(&r, 0x01e8u + 66536u);
    assert(r.state == COLLECTING && recorder_period(&r, 1) == 66536);
}

int main(void)
{
    check_history_and_thresholds();
    puts("PASS  ring order, thresholds, post-trigger length, frozen snapshot");
    check_rearm_and_early_trigger();
    puts("PASS  first edge, early trigger, overcapture and re-arm");
    check_timer_rollover();
    puts("PASS  simultaneous overflow/capture, 32-bit wrap, long period");
    return 0;
}
