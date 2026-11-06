/**
 * @file    perf.c
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Performance monitoring and optimization.
 *
 * @version 1.0
 */
#include <stdio.h>
#include "perf.h"

/**
 * @brief Initializes the performance statistics tracker.
 *
 * @param p Pointer to the PerfStats structure to initialize.
 * @param now The current monotonic time in DDS format.
 */
void perf_init(PerfStats *p, dds_time_t now)
{
    p->period_start = now;
    p->ticks = 0;
    p->work_total = 0;
    p->work_max = 0;
}

/**
 * @brief Records performance statistics for a single simulation tick.
 *
 * @param p Pointer to the PerfStats structure.
 * @param tick_start The start time of the tick in DDS format.
 * @param tick_end The end time of the tick in DDS format.
 * @param players The current number of players in the game.
 */
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
