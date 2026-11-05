#ifndef PERF_H
#define PERF_H

#include <stdint.h>
#include "dds/dds.h"

#define PERF_PERIOD DDS_SECS(10)

typedef struct
{
    dds_time_t     period_start;
    uint64_t       ticks;
    dds_duration_t work_total;
    dds_duration_t work_max;
} PerfStats;

void perf_init(PerfStats *p, dds_time_t now);
void perf_record(PerfStats *p, dds_time_t tick_start, dds_time_t tick_end, int players);

#endif
