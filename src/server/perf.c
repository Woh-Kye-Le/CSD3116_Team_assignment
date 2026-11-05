#include <stdio.h>
#include "perf.h"

void perf_init(PerfStats *p, dds_time_t now)
{
    p->period_start = now;
    p->ticks = 0;
    p->work_total = 0;
    p->work_max = 0;
}

void perf_record(PerfStats *p, dds_time_t tick_start, dds_time_t tick_end, int players)
{
    dds_duration_t work = tick_end - tick_start;
    p->ticks++;
    p->work_total += work;
    if (work > p->work_max)
        p->work_max = work;

    if (tick_end - p->period_start < PERF_PERIOD)
        return;

    double secs = (double)(tick_end - p->period_start) / DDS_NSECS_IN_SEC;
    printf("[perf] %.1f ticks/s | work per tick: avg %.1f us, max %.1f us | players: %d\n",
           p->ticks / secs,
           p->work_total / (double)p->ticks / 1000.0,
           p->work_max / 1000.0, players);
    perf_init(p, tick_end);
}
