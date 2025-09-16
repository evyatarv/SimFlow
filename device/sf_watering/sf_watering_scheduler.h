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
#include <stdint.h>

#define SF_WATERING_USER_DATA_SIZE  sizeof(uint32_t)

// Maximum length for area name
#define MAX_AREA_SIZE   20

typedef struct sf_watering_scheduler_info
{
    char                        area[MAX_AREA_SIZE];    // Area name or identifier
    uint32_t                    id;                     // Unique ID for the schedule
}
sf_watering_scheduler_info_t;

/**
 * @brief Add a new watering schedule.
 *
 * This function creates and adds a new watering schedule for a specified area.
 * The schedule is defined by cron expressions for start and stop times, and can include user data.
 *
 * @param start_cron_exp  Cron expression for when to start watering (e.g., "0 30 8 * * MON,WED,FRI").
 * @param stop_cron_exp   Cron expression for when to stop watering.
 * @param area            Name or identifier for the area to water.
 * @param area_zise       Length of the area name (should not exceed MAX_AREA_SIZE).
 * @param data            Pointer to user data associated with the schedule (can be NULL).
 * @param data_size       Size of the user data in bytes.
 * @param schedule_id     Pointer to an integer where the unique schedule ID will be stored.
 * @return sf_err_t       SF_OK on success, SF_FAIL on error.
 *
 * @note The function allocates memory for the schedule and user data (if provided).
 *       The caller is responsible for managing the returned schedule ID.
 */
sf_err_t sf_watering_add_schdule(const char* start_cron_exp, const char* stop_cron_exp, const char* area, uint8_t area_zise, void* data, uint32_t data_size, int* schedule_id);

/**
 * @brief Remove a watering schedule by its unique ID.
 *
 * @param id      The unique ID of the schedule to remove.
 * @return sf_err_t SF_OK on success, SF_FAIL on error.
 */
sf_err_t sf_watering_remove_schdule(int id);

/**
 * @brief Pause a watering schedule by its unique ID.
 *
 * This function temporarily disables the specified watering schedule without removing it.
 *
 * @param id      The unique ID of the schedule to pause.
 * @return sf_err_t SF_OK on success, SF_FAIL on error.
 */
sf_err_t sf_watering_puse_schedule(int id);

/**
 * @brief Retrieve information about all active watering schedules.
 *
 * This function allocates a buffer and fills it with information about the currently active watering schedules.
 * The caller is responsible for freeing the allocated buffer.
 *
 * @param info                Pointer to a pointer that will be set to the allocated array of sf_watering_scheduler_info_t structures.
 * @param num_of_schedulers   Maximum number of schedules to retrieve.
 * @return sf_err_t           SF_OK on success, SF_FAIL on error (e.g., if info is NULL or allocation fails).
 *
 * @note The caller must free the buffer pointed to by *info after use.
 */
sf_err_t sf_watering_get_schedules(sf_watering_scheduler_info_t** info, uint8_t num_of_schedulers);


#endif // SF_WATERING_SCHEDULER_H

