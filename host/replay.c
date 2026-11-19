/* Feed the firmware recorder a deterministic edge stream without a board. */
#include <inttypes.h>
#include <stdio.h>
#include "recorder.h"

int main(void)
{
    Recorder r;
    uint32_t timestamp = 61000;
    const unsigned jitter[] = {998, 1000, 1002, 999, 1001};
    recorder_arm(&r);
    recorder_edge(&r, timestamp);
    for (unsigned i = 0; i < 60; ++i) {
        unsigned period = (i == 48) ? 1500 : jitter[i % 5];
        timestamp += period;
        recorder_edge(&r, timestamp);
    }
    if (r.state != FROZEN)
        return 1;
    puts("relative_index,period_us");
    for (unsigned i = 0; i < r.count; ++i)
        printf("%d,%" PRIu32 "\n", (int)i - (int)r.pre_count, recorder_period(&r, i));
    return 0;
}
