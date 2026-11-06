/**
 * @file    perf.h
 * @author  Woh Kye Le
 * @date    2026-11-04
 *
 * @brief   Performance monitoring and optimization.
 *
 * @version 1.0
 */
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

/**
 * @brief Initializes the performance statistics tracker.
 *
 * @param p Pointer to the PerfStats structure to initialize.
 * @param now The current monotonic time in DDS format.
 */
void perf_init(PerfStats *p, dds_time_t now);

/**
 * @brief Records performance statistics for a single simulation tick.
 *
 * @param p Pointer to the PerfStats structure.
 * @param tick_start The start time of the tick in DDS format.
 * @param tick_end The end time of the tick in DDS format.
 * @param players The current number of players in the game.
 */
void perf_record(PerfStats *p, dds_time_t tick_start, dds_time_t tick_end, int players);

#endif
