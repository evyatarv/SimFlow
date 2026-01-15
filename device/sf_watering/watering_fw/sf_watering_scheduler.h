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

#pragma pack(push, 1)
typedef struct sf_watering_scheduler_info
{
    uint32_t                    id;                     // Unique ID for the schedule
    char                        area[MAX_AREA_SIZE];    // Area name or identifier
}
sf_watering_scheduler_info_t;
#pragma pack(pop)


/**
 * @brief Add a new watering schedule.
 *
 * This function creates and adds a new watering schedule for a specified area.
 * The schedule is defined by cron expressions for start and stop times, and can include user data.
 *
 * @param start_cron_exp  Cron expression for when to start watering (e.g., "0 30 8 * * MON,WED,FRI").
 * @param stop_cron_exp   Cron expression for when to stop watering.
 * @param area            Name or identifier for the area to water.
 * @param area_size       Length of the area name (should not exceed MAX_AREA_SIZE).
 * @param data            Pointer to user data associated with the schedule (can be NULL).
 * @param data_size       Size of the user data in bytes.
 * @param schedule_id     Pointer to an integer where the unique schedule ID will be stored.
 * @return sf_err_t       SF_OK on success, SF_FAIL on error.
 *
 * @note The function allocates memory for the schedule and user data (if provided).
 *       The caller is responsible for managing the returned schedule ID.
 */
sf_err_t sf_watering_add_schdule(const char* start_cron_exp, const char* stop_cron_exp, const char* area, uint8_t area_size, void* data, uint32_t data_size, int* schedule_id);

/**
 * @brief Remove a watering schedule by its unique ID.
 *
 * @param id      The unique ID of the schedule to remove.
 * @return sf_err_t SF_OK on success, SF_FAIL on error.
 */
sf_err_t sf_watering_remove_schdule(uint32_t id);

sf_err_t sf_watering_print_schedule();
/* Print the current watering schedule */

/**
 * @brief Pause a watering schedule by its unique ID.
 *
 * This function temporarily disables the specified watering schedule without removing it.
 *
 * @param id      The unique ID of the schedule to pause.
 * @return sf_err_t SF_OK on success, SF_FAIL on error.
 */
sf_err_t sf_watering_pause_schedule(int id);

/**
 * @brief Get the size of the serialized schedules data buffer.
 *
 * This function calculates the total size required to store all active watering schedules
 * in serialized format. Use this to allocate an appropriately sized buffer before calling
 * sf_watering_get_schedule_list().
 *
 * @return uint32_t  The size in bytes required to store all schedules, or 0 if no schedules exist.
 */
uint32_t sf_watering_get_schedules_data_size();

/**
 * @brief Retrieve a serialized list of all watering schedules.
 * 
 * This function fills a provided buffer with a serialized representation of all active watering schedules.
 * The caller must allocate the buffer and provide its size.
 * 
 * @param list  Pointer to a buffer where the serialized schedule list will be stored.
 *              The buffer must be allocated by the caller with at least 'size' bytes available.
 * @param size  Size of the buffer in bytes. Should be obtained from sf_watering_get_schedules_data_size().
 * @return sf_err_t  SF_OK on success, SF_FAIL on error (e.g., buffer too small).
 *
 * @details Returned data structure (when SF_OK is returned):
 *          - Offset 0-3 (4 bytes):      Size of data one schedule
 *          - For each schedule:
 *            - Area name (string, null-terminated, up to MAX_AREA_SIZE bytes)
 *            - Schedule ID (4 bytes, from sf_watering_scheduler_info_t)
 *            - Start cron expression (string, null-terminated)
 *            - Stop cron expression (string, null-terminated)
 *
 * @note Buffer allocation/deallocation is the caller's responsibility.
 * @see sf_watering_get_schedules_data_size() to determine the required buffer size.
 */
sf_err_t sf_watering_get_schedule_list(uint8_t* list, uint32_t size);

#endif // SF_WATERING_SCHEDULER_H

