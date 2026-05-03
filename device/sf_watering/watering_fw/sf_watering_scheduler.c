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
#include "sf_files.h"


// Tag for logging
static const char* TAG = "SF_WATTERING_SCHEDULER";


// Structure representing a watering schedule job
typedef struct sf_watering_scheduler
{
    cron_job*                       start_handle;           // Handle for the cron job that starts watering
    cron_job*                       stop_handle;            // Handle for the cron job that stops watering
    void*                           data;                   // user data 
    sf_watering_scheduler_info_t    info;                   // scheduler info
    struct sf_watering_scheduler*   next_schedule;          // Pointer to the next schedule in the linked list
}
sf_watering_scheduler_t;


typedef struct sf_watering_scheduler_data
{
    uint32_t num_of_schedules;  // Number of watering schedules
    sf_watering_scheduler_t* watering_jobs_head; // Head pointer for the linked list of watering jobs
}sf_watering_scheduler_data_t;


// saves watering schedules data 
static sf_watering_scheduler_data_t watering_jobs_data = {0};


static sf_err_t sf_watering_file_delete(const char* file_path, uint32_t id)
{
    sf_err_t status = SF_FAIL;
    uint32_t num_schedules = 0;
    uint32_t size = sizeof(uint32_t);
    sf_watering_schedule_file_entry_t* entries = NULL;

    sf_file_read(file_path, (uint8_t*)&num_schedules, &size, 0);
    if (num_schedules == 0)
    {
        ESP_LOGE(TAG, "No schedules in file to delete");
        return SF_FAIL;
    }

    entries = calloc(num_schedules, sizeof(*entries));
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, entries, end, "Failed to allocate entries buffer");

    // Read all entries
    size = num_schedules * sizeof(*entries);
    status = sf_file_read(file_path, (uint8_t*)entries, &size, sizeof(uint32_t));
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, status, end, "Read entries from file end with status: %d", status);

    // Find and remove by shifting
    uint32_t new_count = 0;
    for (uint32_t i = 0; i < num_schedules; i++)
    {
        if (entries[i].id == id) continue;
        entries[new_count++] = entries[i];
    }

    if (new_count == num_schedules)
    {
        ESP_LOGE(TAG, "Schedule id: %lu not found in file", id);
        status = SF_FAIL;
        goto end;
    }

    // Rewrite count
    size = sizeof(uint32_t);
    status = sf_file_write(file_path, (uint8_t*)&new_count, &size, 0);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, end, "Write updated count with status: %d", status);

    // Rewrite remaining entries
    if (new_count > 0)
    {
        size = new_count * sizeof(*entries);
        status = sf_file_write(file_path, (uint8_t*)entries, &size, sizeof(uint32_t));
        SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, end, "Rewrite entries with status: %d", status);
    }

    ESP_LOGI(TAG, "Schedule id: %lu deleted from file, remaining: %lu", id, new_count);

end:
    free(entries);
    return status;
}

static sf_err_t sf_watering_file_add(const char* file_path, uint32_t id,
                                      const char* start_cron, const char* stop_cron,
                                      const char* area)
{
    sf_err_t status = SF_FAIL;
    uint32_t num_schedules = 0;
    uint32_t size = sizeof(uint32_t);
    sf_watering_schedule_file_entry_t entry = {0};

    // Read current count — ignore error, file may not exist yet (num_schedules stays 0)
    sf_file_read(file_path, (uint8_t*)&num_schedules, &size, 0);

    // Update count
    uint32_t entry_offset = sizeof(uint32_t) + num_schedules * sizeof(entry);
    num_schedules++;
    size = sizeof(uint32_t);
    status = sf_file_write(file_path, (uint8_t*)&num_schedules, &size, 0);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, end, "Update schedule count in file with status: %d", status);

    // Fill entry
    entry.id = id;
    strncpy(entry.area, area, MAX_AREA_SIZE - 1);
    strncpy(entry.start_cron, start_cron, MAX_CRON_STR_SIZE - 1);
    strncpy(entry.stop_cron, stop_cron, MAX_CRON_STR_SIZE - 1);

    // Write entry at end of file
    size = sizeof(entry);
    status = sf_file_write(file_path, (uint8_t*)&entry, &size, entry_offset);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, end, "Write schedule entry to file with status: %d", status);

    ESP_LOGI(TAG, "Schedule id: %lu saved to file, total: %lu", id, num_schedules);

end:
    return status;
}

static uint32_t sf_watering_get_schedule_entry_size()
{
    uint32_t entry_size = 0;

    //Size to allocate for this schedule entry id|area|start_expr|stop_expr
    entry_size = sizeof(sf_watering_scheduler_info_t) + sizeof(cron_expr)*2;

    return entry_size;
}


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
    sf_gpio_set_level(CONFIG_GPIO_OUTPUT_0, 1); // set GPIO level
    ESP_LOGI(TAG, "Watering start: %s", sf_time_get_current_time());
}


// Callback to turn off the GPIO (stop watering)
void sf_watering_gpio_off_cb(cron_job *job)
{
    sf_gpio_set_level(CONFIG_GPIO_OUTPUT_0, 0); // set GPIO level
    ESP_LOGI(TAG, "Watering end: %s", sf_time_get_current_time());
}


// Internal: create and register a schedule in memory without file persistence
static sf_err_t sf_watering_create_schedule(uint32_t id, const char* start_cron_exp, const char* stop_cron_exp, const char* area, uint8_t area_zise, void* data, uint32_t data_size)
{
    sf_err_t status = SF_FAIL;
    sf_watering_scheduler_t* new_schedule = 0;
    sf_watering_scheduler_t** watering_jobs_curr = &watering_jobs_data.watering_jobs_head;

    if (start_cron_exp == NULL || stop_cron_exp == NULL || area == NULL || area_zise > MAX_AREA_SIZE)
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
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, new_schedule->start_handle, FAIL, "fail allocate start cron job");

    // Create cron job for stopping watering
    new_schedule->stop_handle = cron_job_create(stop_cron_exp, sf_watering_gpio_off_cb, new_schedule->data);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, new_schedule->stop_handle, FAIL, "fail allocate stop cron job");

    // Use the server-assigned stable ID
    new_schedule->info.id = id;

    // Insert the new schedule at the end of the linked list
    while (*watering_jobs_curr != NULL)
    {
        watering_jobs_curr = &(*watering_jobs_curr)->next_schedule;
    }
    *watering_jobs_curr = new_schedule;

    status = SF_OK;

FAIL:
    if (status != SF_OK)
    {
        sf_wattering_clean_schedule_resourses(new_schedule);
    }

    watering_jobs_data.num_of_schedules++;

    return status;
}


// Adds a new watering schedule to the linked list and persists it to file
sf_err_t sf_watering_add_schdule(uint32_t id, const char* start_cron_exp, const char* stop_cron_exp, const char* area, uint8_t area_zise, void* data, uint32_t data_size)
{
    cron_stop();

    sf_err_t status = sf_watering_create_schedule(id, start_cron_exp, stop_cron_exp, area, area_zise, data, data_size);

    if (status == SF_OK)
    {
        sf_err_t cron_status = cron_start();
        ESP_LOGI(TAG, "Cron start returned: %d", cron_status);

        // Persist to file — non-fatal, log warning only
        if (sf_watering_file_add(SF_WATERING_SCHEDULE_FILE, id, start_cron_exp, stop_cron_exp, area) != SF_OK)
        {
            ESP_LOGW(TAG, "Failed to persist schedule id: %lu to file", id);
        }
    }

    return status;
}

// Removes a watering schedule by ID
sf_err_t sf_watering_remove_schdule(uint32_t id)
{
    sf_err_t status = SF_FAIL;

    // Pointers for traversing the linked list
    sf_watering_scheduler_t* curr = watering_jobs_data.watering_jobs_head; 
    sf_watering_scheduler_t* prev = NULL;

    // Traverse the list to find the schedule with the given ID
    while (curr != NULL)
    {
        if (curr->info.id == id)
        {
            // Remove the node from the linked list
            if (prev != NULL)
            {
                prev->next_schedule = curr->next_schedule;
                curr->next_schedule = NULL;
            }
            else
            {
                watering_jobs_data.watering_jobs_head = curr->next_schedule;
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
        watering_jobs_data.num_of_schedules--; //TODO cant be less than 0?

        // Persist deletion to file — non-fatal, log warning only
        if (sf_watering_file_delete(SF_WATERING_SCHEDULE_FILE, id) != SF_OK)
        {
            ESP_LOGW(TAG, "Failed to delete schedule id: %lu from file", id);
        }
    }

    ESP_LOGI(TAG, "Remove DONE with status: %d, num of schedules: %lu", status, watering_jobs_data.num_of_schedules);

    return status;
}

uint32_t sf_watering_get_schedules_data_size()
{
    uint32_t entry_size = 0;
    uint32_t data_size = 0;

    // Checking list is not empty
    if (watering_jobs_data.watering_jobs_head == NULL)
    {
        ESP_LOGI(TAG, "No watering schedules found.");
        return 0;
    }

    entry_size = sf_watering_get_schedule_entry_size();

    //calc total data size entry_size|data
    data_size = (watering_jobs_data.num_of_schedules * entry_size);

    return data_size;
}


sf_err_t sf_watering_get_schedule_list(uint8_t* list, uint32_t size){
    uint32_t data_counter = 0;
    uint32_t data_size = 0;

    sf_watering_scheduler_t* curr = watering_jobs_data.watering_jobs_head; 

    data_size = sf_watering_get_schedules_data_size();

    if (list == NULL || size < data_size + sizeof(uint32_t))
    {
        ESP_LOGE(TAG, "list size too small or NULL");
        return SF_FAIL;
    }

    // copy num of entries
    memcpy(list, &watering_jobs_data.num_of_schedules, sizeof(uint32_t));
    data_counter += sizeof(uint32_t);

    while (curr != NULL)  
    {
        // copy schedule info 
        memcpy(list + data_counter, &curr->info , sizeof(curr->info));
        data_counter += sizeof(curr->info);

        // copy start schedule expr.
        memcpy(list + data_counter, &curr->start_handle->expression , sizeof(curr->start_handle->expression));
        data_counter += sizeof(curr->start_handle->expression);

        // copy stop schedule expr. 
        memcpy(list + data_counter, &curr->stop_handle->expression , sizeof(curr->stop_handle->expression));
        data_counter += sizeof(curr->stop_handle->expression);

        curr = curr->next_schedule;   
    }

    return SF_OK;

}

sf_err_t sf_watering_print_schedule()
{
    sf_err_t status = SF_FAIL;

    // Pointer for traversing the linked list
    sf_watering_scheduler_t* curr = watering_jobs_data.watering_jobs_head; 
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
    sf_watering_scheduler_t* curr = watering_jobs_data.watering_jobs_head;

    // Traverse the list to find the schedule with the given ID
    while (curr != NULL)
    {
        if (curr->info.id == id)
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


sf_err_t sf_watering_load_from_file(const char* file_path)
{
    sf_err_t status = SF_FAIL;
    uint32_t num_schedules = 0;
    uint32_t size = sizeof(uint32_t);
    sf_watering_schedule_file_entry_t* entries = NULL;

    status = sf_file_read(file_path, (uint8_t*)&num_schedules, &size, 0);
    if (status != SF_OK || num_schedules == 0)
    {
        ESP_LOGI(TAG, "No schedules to restore from file");
        return SF_OK;
    }

    entries = calloc(num_schedules, sizeof(*entries));
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, entries, end, "Failed to allocate entries buffer");

    size = num_schedules * sizeof(*entries);
    status = sf_file_read(file_path, (uint8_t*)entries, &size, sizeof(uint32_t));
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, end, "Read entries from file end with status: %d", status);

    ESP_LOGI(TAG, "Restoring %lu schedule(s) from file", num_schedules);

    for (uint32_t i = 0; i < num_schedules; i++)
    {
        if (sf_watering_create_schedule(entries[i].id, entries[i].start_cron, entries[i].stop_cron,
                                         entries[i].area, (uint8_t)strnlen(entries[i].area, MAX_AREA_SIZE),
                                         NULL, 0) != SF_OK)
        {
            ESP_LOGE(TAG, "Failed to restore schedule id: %lu", entries[i].id);
        }
        else
        {
            ESP_LOGI(TAG, "Restored schedule id: %lu, area: %s", entries[i].id, entries[i].area);
            ESP_LOGI(TAG, "Start schedule expr: %s", entries[i].start_cron);
            ESP_LOGI(TAG, "Stop schedule expr: %s", entries[i].stop_cron);
        }
    }

    sf_err_t cron_status = cron_start();
    ESP_LOGI(TAG, "Cron start returned: %d", cron_status);

    status = SF_OK;

end:
    free(entries);
    return status;
}


sf_err_t sf_watering_get_file_schedule_list(uint8_t* out_buf, uint32_t* out_size)
{
    sf_err_t status = SF_FAIL;
    uint32_t num_schedules = 0;
    uint32_t size = sizeof(uint32_t);

    
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, out_size, end, "out size cant be NULL");
    
    *out_size = 0;

    // Read count — non-fatal if file doesn't exist yet (first boot), num_schedules stays 0
    status = sf_file_read(SF_WATERING_SCHEDULE_FILE, (uint8_t*)&num_schedules, &size, 0);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != SF_OK, end, 
        "Failed to read schedule count from file with status: %d", status);
  
    *out_size = sizeof(uint32_t) + (num_schedules * sizeof(sf_watering_schedule_file_entry_t));
    ESP_LOGI(TAG, "Schedule file: %lu schedule(s), buffer size: %lu", num_schedules, *out_size);

    // First call: caller passes NULL to query required size only
    if (out_buf == NULL || num_schedules == 0)
    {
        status = SF_OK;
        goto end;
    }

    // Read entire file (count + entries) in one shot directly into the output buffer
    status = sf_file_read(SF_WATERING_SCHEDULE_FILE, out_buf, out_size, 0);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, status, end, "Failed to read schedule file with status: %d", status);

    ESP_LOGI(TAG, "Schedule file read: %lu entries, %lu bytes", num_schedules, *out_size);
    status = SF_OK;

end:
    return status;
}

