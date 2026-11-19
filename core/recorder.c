#include "recorder.h"

void recorder_arm(Recorder *r)
{
    /* Old entries become unreachable. No buffer clear in the critical section. */
    r->state = ARMED;
    r->have_edge = false;
    r->next = 0;
    r->count = 0;
    r->pre_count = 0;
    r->post_count = 0;
}

uint32_t capture_timestamp(uint32_t epoch, uint16_t capture, bool update_pending)
{
    if (update_pending && capture < 0x8000u)
        epoch += 0x10000u;
    return epoch + capture;
}

void recorder_edge(Recorder *r, uint32_t timestamp_us)
{
    if (r->state == FROZEN || r->state == CAPTURE_LOST)
        return;

    if (!r->have_edge) {
        r->previous_edge = timestamp_us;
        r->have_edge = true;
        return;
    }

    uint32_t period = timestamp_us - r->previous_edge;
    r->previous_edge = timestamp_us;
    r->periods[r->next] = period;
    r->next = (r->next + 1u) % WINDOW_SIZE;

    if (r->state == ARMED) {
        if (period >= MIN_PERIOD_US && period <= MAX_PERIOD_US) {
            if (r->count < PRE_PERIODS)
                ++r->count;
            return;
        }
        r->pre_count = r->count;
        ++r->count;
        r->state = COLLECTING;
        return;
    }

    ++r->count;
    if (++r->post_count == POST_PERIODS)
        r->state = FROZEN;
}

void recorder_lost(Recorder *r)
{
    if (r->state != FROZEN)
        r->state = CAPTURE_LOST;
}

uint32_t recorder_period(const Recorder *r, unsigned index)
{
    return r->periods[(r->next + WINDOW_SIZE - r->count + index) % WINDOW_SIZE];
}
