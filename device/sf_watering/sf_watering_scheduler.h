#ifndef SF_WATERING_SCHEDULER_H
#define SF_WATERING_SCHEDULER_H

/**
 * @file sf_watering_scheduler.h
 * @brief Interface for managing watering schedules using cron expressions.
 *
 * This header provides functions to add and remove watering schedules for different areas.
 * Schedules are defined by cron expressions for start and stop times, and are associated with a specific area.
 */

#include "sf_err.h"

#define SF_WATERING_USER_DATA_SIZE  sizeof(uint32_t)

sf_err_t sf_watering_remove_schdule(int id);

/**
 * @brief Add a new watering schedule.
 *
 * @param start_cron_exp  Cron expression for when to start watering (e.g., "0 30 8 * * MON,WED,FRI").
 * @param stop_cron_exp   Cron expression for when to stop watering.
 * @param area            Name or identifier for the area to water.
 * @param area_zise       Length of the area name (should not exceed MAX_AREA_SIZE).
 * @return sf_err_t       SF_OK on success, SF_FAIL on error.
 */
sf_err_t sf_watering_add_schdule(const char* start_cron_exp, const char* stop_cron_exp, const char* area, uint8_t area_zise, void* data, uint32_t data_size);

/**
 * @brief Remove a watering schedule by its unique ID.
 *
 * @param id      The unique ID of the schedule to remove.
 * @return sf_err_t SF_OK on success, SF_FAIL on error.
 */
sf_err_t sf_watering_remove_schdule(int id);

sf_err_t sf_watering_puse_schedule(int id);

/**
 * @brief Pause a watering schedule by its unique ID.
 *
 * This function temporarily disables the specified watering schedule without removing it.
 *
 * @param id      The unique ID of the schedule to pause.
 * @return sf_err_t SF_OK on success, SF_FAIL on error.
 */
sf_err_t sf_watering_puse_schedule(int id);

#endif // SF_WATERING_SCHEDULER_H

