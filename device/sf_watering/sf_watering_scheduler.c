#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "sf_watering_scheduler.h"
#include "cron.h"
#include "sf_err.h"
#include "sf_gpio.h"

// Maximum length for area name
#define MAX_AREA_SIZE   20

// Tag for logging
static const char* TAG = "SF_WATTERING_SCHEDULER";




// Forward declaration for the scheduler struct
typedef struct sf_watering_scheduler sf_watering_scheduler_t;


// Structure representing a watering schedule job
struct sf_watering_scheduler
{
    cron_job* start_handle;              // Handle for the cron job that starts watering
    cron_job* stop_handle;               // Handle for the cron job that stops watering
    char area[MAX_AREA_SIZE];            // Area name or identifier
    uint32_t id;                         // Unique ID for the schedule
    void* data;                          // user data 
    sf_watering_scheduler_t* next_schedule; // Pointer to the next schedule in the linked list
};


// Head pointer for the linked list of watering jobs
static sf_watering_scheduler_t* watering_jobs_head = NULL;

static void sf_wattering_clean_schedule_resourses(sf_watering_scheduler_t* schedule)
{
    sf_err_t status = SF_FAIL; 

    if(schedule == NULL)
        return; 

    if (schedule->data != NULL)
        free(schedule->data);

    // Clear area string
    memset(schedule->area, 0, MAX_AREA_SIZE);

    // Destroy the cron jobs for this schedule
    status = cron_job_destroy(schedule->start_handle);
    ESP_LOGI(TAG, "cron_job_destroy for start_handle status: %d", status);

    status = cron_job_destroy(schedule->stop_handle);
    ESP_LOGI(TAG, "cron_job_destroy for stop_handle status: %d", status);

    schedule->id = -1;

    free(schedule);
}


// Callback to turn on the GPIO (start watering)
void sf_watering_gpio_on_cb(cron_job *job)
{
    SF_CHECK_NULL_RETURN(ESP_LOGE, TAG, job->data, "GPIO ON: User data can't be null")

    sf_gpio_set_level(*(uint32_t*)job->data, 1); // set GPIO level
}


// Callback to turn off the GPIO (stop watering)
void sf_watering_gpio_off_cb(cron_job *job)
{
    SF_CHECK_NULL_RETURN(ESP_LOGE, TAG, job->data, "GPIO OFF: User data can't be null")

    sf_gpio_set_level(*(uint32_t*)job->data, 0); // set GPIO level
}


// Adds a new watering schedule to the linked list
sf_err_t sf_watering_add_schdule(const char* start_cron_exp, const char* stop_cron_exp, const char* area, uint8_t area_zise, void* data, uint32_t data_size)
{
    sf_err_t status = SF_FAIL;
    sf_watering_scheduler_t* new_schedule = 0;
    sf_watering_scheduler_t* watering_jobs_curr = watering_jobs_head;
 
    if (start_cron_exp == NULL || stop_cron_exp == NULL || area == NULL || area_zise > MAX_AREA_SIZE)
    {
        ESP_LOGE(TAG, "wrong param: start_cron_exp:%p stop_cron_exp:%p area: %p area_zise: %u", 
            start_cron_exp, stop_cron_exp, area, area_zise);
        return SF_FAIL;
    }

    // Allocate memory for the new schedule
    new_schedule = (sf_watering_scheduler_t*)calloc(sizeof(sf_watering_scheduler_t), 1);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, new_schedule, FAIL, "fail allocate sf_watering_scheduler");

    // Copy area string into struct and ensure null-termination
    strncpy(new_schedule->area, area, MAX_AREA_SIZE - 1);
    new_schedule->area[MAX_AREA_SIZE - 1] = '\0';

    if (data != NULL && data_size > SF_WATERING_USER_DATA_SIZE)
    {
        new_schedule->data = calloc(data_size, 1); 
        SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, new_schedule, FAIL, "fail allocate user data");

        memcpy(new_schedule->data, data, data_size); 
    }
    // Create cron job for starting watering
    new_schedule->start_handle = cron_job_create(start_cron_exp, sf_watering_gpio_on_cb, new_schedule->data);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, new_schedule, FAIL,"fail allocate strat cron job");

    // Create cron job for stopping watering
    new_schedule->stop_handle = cron_job_create(stop_cron_exp, sf_watering_gpio_off_cb, new_schedule->data);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, new_schedule, FAIL,"fail allocate stop cron job");

    // Use the start cron job's ID as the schedule ID
    new_schedule->id = new_schedule->start_handle->id;

    // save new schedule 
    // Insert the new schedule at the end of the linked list
    while (watering_jobs_curr != NULL)
    {
        watering_jobs_curr = watering_jobs_curr->next_schedule;
    }
    watering_jobs_curr = new_schedule; 
    
    status = SF_OK;
    
FAIL:

    if (status != SF_OK)
    {
        sf_wattering_clean_schedule_resourses(new_schedule);
    }

    return status;
}

// Removes a watering schedule by ID
sf_err_t sf_wwatering_remove_schdule(int id)
{
    sf_err_t status = SF_FAIL;

    // Pointers for traversing the linked list
    sf_watering_scheduler_t* curr = watering_jobs_head; 
    sf_watering_scheduler_t* prev = NULL;


    // Traverse the list to find the schedule with the given ID
    while (curr != NULL)
    {
        if (curr->start_handle->id == id)
        {
            // Remove the node from the linked list
            if (prev != NULL)
            {
                prev->next_schedule = curr->next_schedule;
            }
            else
            {
                watering_jobs_head = curr->next_schedule;
            }

            sf_wattering_clean_schedule_resourses(curr);
        }

        prev = curr; 
        curr = curr->next_schedule;
    }

    return status;
}


sf_err_t sf_watering_puse_schedule(int id)
{
    sf_err_t status = SF_FAIL;

    // Pointer for traversing the linked list
    sf_watering_scheduler_t* curr = watering_jobs_head; 

    // Traverse the list to find the schedule with the given ID
    while (curr != NULL)
    {
        if (curr->start_handle->id == id)
        {
            // Unschedule the cron jobs for this schedule
            status = cron_job_unschedule(curr->start_handle);
            ESP_LOGI(TAG, "cron_job_unschedule for start_handle status: %d", status);

            status += cron_job_unschedule(curr->stop_handle);
            ESP_LOGI(TAG, "cron_job_unschedule for stop_handle status: %d", status);

            SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, status, FAIL, "Failed unschedule watering %d", curr->start_handle->id);

            break;
        }

        curr = curr->next_schedule;
    }

FAIL:
    return status;
}

