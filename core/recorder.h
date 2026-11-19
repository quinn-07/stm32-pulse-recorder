#ifndef PULSE_RECORDER_H
#define PULSE_RECORDER_H

#include <stdbool.h>
#include <stdint.h>

enum {
    PRE_PERIODS = 16,
    POST_PERIODS = 8,
    WINDOW_SIZE = PRE_PERIODS + 1 + POST_PERIODS,
    MIN_PERIOD_US = 900,
    MAX_PERIOD_US = 1100
};

typedef enum { ARMED, COLLECTING, FROZEN, CAPTURE_LOST } CaptureState;

typedef struct {
    uint32_t periods[WINDOW_SIZE];
    uint32_t previous_edge;
    bool have_edge;
    unsigned next;
    unsigned count;
    unsigned pre_count;
    unsigned post_count;
    CaptureState state;
} Recorder;

void recorder_arm(Recorder *r);
void recorder_edge(Recorder *r, uint32_t timestamp_us);
void recorder_lost(Recorder *r);
/* Caller supplies an index less than count. Index 0 is the oldest period. */
uint32_t recorder_period(const Recorder *r, unsigned index);

/* epoch is the aligned upper word before servicing the current update flag.
   Timer IRQ must be serviced in less than half a 16-bit timer cycle. */
uint32_t capture_timestamp(uint32_t epoch, uint16_t capture, bool update_pending);
#endif
