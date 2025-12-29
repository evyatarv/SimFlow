#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "sf_watering_scheduler.h"
#include "cron.h"
#include "sf_err.h"
#include "sf_gpio.h"
#include "sf_gpio_cfg.h"
#include "sf_time.h"


// Tag for logging
static const char* TAG = "SF_WATTERING_SCHEDULER";
int num_of_schedules = 0;


// Forward declaration for the scheduler struct
typedef struct sf_watering_scheduler sf_watering_scheduler_t;


// Structure representing a watering schedule job
struct sf_watering_scheduler
{
    cron_job*                       start_handle;           // Handle for the cron job that starts watering
    cron_job*                       stop_handle;            // Handle for the cron job that stops watering
    void*                           data;                   // user data 
    sf_watering_scheduler_info_t    info;                   // scheduler info
    sf_watering_scheduler_t*        next_schedule;          // Pointer to the next schedule in the linked list
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
    memset(schedule->info.area, 0, MAX_AREA_SIZE);

    // Destroy the cron jobs for this schedule
    status = cron_job_destroy(schedule->start_handle);
    ESP_LOGI(TAG, "cron_job_destroy for start_handle status: %d", status);

    status = cron_job_destroy(schedule->stop_handle);
    ESP_LOGI(TAG, "cron_job_destroy for stop_handle status: %d", status);

    schedule->info.id = -1;

    free(schedule);
}


// Callback to turn on the GPIO (start watering)
void sf_watering_gpio_on_cb(cron_job *job)
{
    sf_gpio_set_level(GPIO_OUTPUT_PIN_SEL, 1); // set GPIO level
    ESP_LOGI(TAG, "Watering start: %s", sf_time_get_current_time());
}


// Callback to turn off the GPIO (stop watering)
void sf_watering_gpio_off_cb(cron_job *job)
{
    sf_gpio_set_level(GPIO_OUTPUT_PIN_SEL, 0); // set GPIO level
    ESP_LOGI(TAG, "Watering end: %s", sf_time_get_current_time());
}


// Adds a new watering schedule to the linked list
sf_err_t sf_watering_add_schdule(const char* start_cron_exp, const char* stop_cron_exp, const char* area, uint8_t area_zise, void* data, uint32_t data_size, int* schedule_id)
{
    sf_err_t status = SF_FAIL;
    sf_watering_scheduler_t* new_schedule = 0;
    sf_watering_scheduler_t** watering_jobs_curr = &watering_jobs_head;

 
    if (start_cron_exp == NULL || stop_cron_exp == NULL || area == NULL || area_zise > MAX_AREA_SIZE || schedule_id == NULL)
    {
        ESP_LOGE(TAG, "wrong param: start_cron_exp:%p stop_cron_exp:%p area: %p area_zise: %u", 
            start_cron_exp, stop_cron_exp, area, area_zise);
        return SF_FAIL;
    }

    // Allocate memory for the new schedule
    new_schedule = (sf_watering_scheduler_t*)calloc(1, sizeof(sf_watering_scheduler_t));
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, new_schedule, FAIL, "fail allocate sf_watering_scheduler");

    // Copy area string into struct and ensure null-termination
    strncpy(new_schedule->info.area, area, MAX_AREA_SIZE - 1);
    new_schedule->info.area[MAX_AREA_SIZE - 1] = '\0';

    if (data != NULL && data_size <= SF_WATERING_USER_DATA_SIZE)
    {
        new_schedule->data = calloc(1, data_size); 
        SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, new_schedule, FAIL, "fail allocate user data");

        memcpy(new_schedule->data, data, data_size); 
    }

    // Create cron job for starting watering
    new_schedule->start_handle = cron_job_create(start_cron_exp, sf_watering_gpio_on_cb, new_schedule->data);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, new_schedule->start_handle, FAIL,"fail allocate strat cron job");

    // Create cron job for stopping watering
    new_schedule->stop_handle = cron_job_create(stop_cron_exp, sf_watering_gpio_off_cb, new_schedule->data);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, new_schedule->stop_handle, FAIL,"fail allocate stop cron job");

    // Use the start cron job's ID as the schedule ID
    new_schedule->info.id = new_schedule->start_handle->id;
    *schedule_id = new_schedule->start_handle->id;

    // save new schedule 
    // Insert the new schedule at the end of the linked list
    while (*watering_jobs_curr != NULL)
    {
        watering_jobs_curr = &(*watering_jobs_curr)->next_schedule;
        
    }
    *watering_jobs_curr = new_schedule; 
    
    // start scheduler
    status =  cron_start();
    ESP_LOGI(TAG, "Cron start returned: %d", status);


    status = SF_OK;
    
FAIL:

    if (status != SF_OK)
    {
        sf_wattering_clean_schedule_resourses(new_schedule);
        *schedule_id = -1; 
    }
    
    num_of_schedules++;

    return status;
}

// Removes a watering schedule by ID
sf_err_t sf_watering_remove_schdule(int id)
{
    sf_err_t status = SF_FAIL;

    // Pointers for traversing the linked list
    sf_watering_scheduler_t* curr = watering_jobs_head; 
    sf_watering_scheduler_t* prev = NULL;

    ESP_LOGI(TAG, "sf_watering_remove_schdule curr = %d ", id);
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

            status = SF_OK;
            break;
        }

        prev = curr; 
        curr = curr->next_schedule;
    }
    if (status == SF_OK)
    {
        num_of_schedules--; //TODO cant be less than 0?
    }
    return status;
}

/*
data to return per schedule
schedule aread STR from sf_watering_scheduler_info_t
schedule ID form sf_watering_scheduler_info_t
schedule expr. from cron_job_struct

returned data stucture:
Size of data  - 4bytes
number of schedules - 4bytes 
data per schedule

*/
sf_err_t sf_watering_get_schedule_list(char* list){
    sf_err_t status = SF_FAIL;
    char* data_per_schedule = NULL;
    int total_size = 0;

    // Pointer for traversing the linked list
    sf_watering_scheduler_t* curr = watering_jobs_head; 
    int counter = 0;

    // Checking list is not empty
    if (curr == NULL)
    {
        ESP_LOGI(TAG, "No watering schedules found.");
        return status;  //TODO what value is expected to return here
    }

    //Size to allocate for this schedule entry
    int size = sizeof(curr->info) + sizeof(curr->start_handle->expression) + sizeof(curr->stop_handle->expression);
    data_per_schedule = (char*)calloc(1, size);

    
    //First time add total size at begining of list
    int size_to_allocate = sizeof(int)*2 + (num_of_schedules * size);

    list = (char*)calloc(1, size_to_allocate);

    //the size of data is less in 8 bytes which are size and number of schedules which needed for allocation
    int size_of_data = size_to_allocate - sizeof(int)*2;
    memcpy(list, &size_of_data, sizeof(int));
    memcpy(list + sizeof(int), &num_of_schedules, sizeof(int));

    while (curr != NULL)
    {
        
        //build data per schedule
        memcpy(data_per_schedule, &curr->info , sizeof(curr->info));
        memcpy(data_per_schedule + sizeof(curr->info), &curr->start_handle->expression , sizeof(curr->start_handle->expression));
        memcpy(data_per_schedule + sizeof(curr->info) + sizeof(curr->start_handle->expression), &curr->stop_handle->expression , sizeof(curr->stop_handle->expression));

        //copy data per schedule to list
        memcpy(list + counter*size + sizeof(int)*2, data_per_schedule, size);

        curr = curr->next_schedule;    
        counter++;

    }

    free(data_per_schedule);

    ESP_LOGI(TAG, "Counter: %d", counter );

    status = SF_OK;
    return status;

}

sf_err_t sf_watering_print_schedule()
{
    sf_err_t status = SF_FAIL;

    // Pointer for traversing the linked list
    sf_watering_scheduler_t* curr = watering_jobs_head; 
    int counter = 0;

    // Traverse the list to find the schedule with the given ID
    if (curr == NULL)
    {
        ESP_LOGI(TAG, "No watering schedules found.");
        return SF_OK;
    }

    ESP_LOGI(TAG, "Current Watering Schedules:");
    while (curr != NULL)
    {
        ESP_LOGI(TAG, "-----------------------------------");
        ESP_LOGI(TAG, "Counter =%d, Schedule ID: %d",counter, (int) curr->info.id);
        if (curr->data != NULL)
        {
            ESP_LOGI(TAG, "User Data: 0x%08X", *(unsigned int*)(curr->data));
        }
        else
        {
            ESP_LOGI(TAG, "User Data: None");
        }
        curr = curr->next_schedule;
        counter++;
    }
    ESP_LOGI(TAG, "Counter: %d", counter );

    status = SF_OK;

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

