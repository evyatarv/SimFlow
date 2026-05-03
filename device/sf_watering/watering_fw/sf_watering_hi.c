#include "sf_watering_hi.h"
#include "sf_watering_scheduler.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"


#define SF_MQTT_NEW_SCHEDULER_TOPIC     "sf_watering/schedule"
#define SF_WATERING_HI_CMD_MIN_SIZE     5
#define SF_WATERING_HI_CMD_QUEUE_DEPTH  5
#define SF_WATERING_HI_WORKER_STACK     8192
#define SF_WATERING_HI_WORKER_PRIORITY  5


// Tag for logging
static const char* TAG = "SF_WATTERING_HI";

static esp_mqtt_client_handle_t sf_watering_mqtt_lient = NULL;
static QueueHandle_t            sf_watering_hi_cmd_queue = NULL;

#pragma pack(push, 1)
typedef struct  sf_watering_hi_cmd
{
    uint8_t     cmd;
    uint32_t    data_size;
    void*       data[];
}
sf_watering_hi_cmd_t;
#pragma pack(pop)


enum SF_SCHEDULER_CMD
{
    SF_WATERING_NEW_CHEDULER = 1, 
    SF_WATERING_REMOVE_SCHEDULER, 
    SF_WATERING_GET_SCHEDULERS
};

static void sf_watering_hi_cmd_parser(void* cmd, size_t data_size);

static void sf_watering_hi_worker_task(void* args)
{
    sf_watering_hi_cmd_t* cmd = NULL;
    while (true)
    {
        if (xQueueReceive(sf_watering_hi_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE)
        {
            sf_watering_hi_cmd_parser(cmd, SF_WATERING_HI_CMD_MIN_SIZE + cmd->data_size);
            free(cmd);
            cmd = NULL;
        }
    }
}

static void sf_watering_hi_cmd_parser(void* cmd, size_t data_size)
{
    sf_err_t status = SF_FAIL;

    sf_watering_hi_cmd_t* watering_cmd = NULL;
    sf_watering_hi_cmd_t* watering_ret = NULL;

    if (cmd == NULL || data_size < SF_WATERING_HI_CMD_MIN_SIZE)
    {
        ESP_LOGE(TAG, "Wrong SF watering HI command format");
        
        return;
    }

    // get sf_watering_hi_cmd_t
    watering_cmd = (sf_watering_hi_cmd_t*)cmd;


    ESP_LOGI(TAG, "cmd %d", watering_cmd->cmd);
    ESP_LOGI(TAG, "data size %d", (int)watering_cmd->data_size);
    ESP_LOGI(TAG, "data %s", (uint8_t*)&watering_cmd->data);

    

    switch (watering_cmd->cmd)
    {
    case SF_WATERING_NEW_CHEDULER:
        
        char* watering_schedule = (char*)&watering_cmd->data;

        int offset = 0;
        int crone_exp_size = 0;
        int area_size = 0;
        char* start_cron_exp = NULL;
        char* stop_cron_exp = NULL;
        char* area_str = NULL;
        uint32_t schedule_id = 0;

        // get server-assigned stable ID
        memcpy(&schedule_id, watering_schedule + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);
        ESP_LOGI(TAG, "schedule ID from server: %lu", schedule_id);

        // get start cron expression size
        crone_exp_size = watering_schedule[offset];

        // get start cron expretion
        offset += 1;
        start_cron_exp = watering_schedule + offset;
        ESP_LOGI(TAG, "cron start exp. %s size: %d", start_cron_exp, crone_exp_size);

        // get cron stop expretion size
        offset += crone_exp_size;
        crone_exp_size = watering_schedule[offset];

        // get stop cron expretion
        offset += 1;
        stop_cron_exp = watering_schedule + offset;
        ESP_LOGI(TAG, "cron stop exp. %s size: %d", stop_cron_exp, crone_exp_size);

        // get area size
        offset += crone_exp_size;
        area_size = watering_schedule[offset];

        // get area str
        offset += 1;
        area_str = watering_schedule + offset;
        ESP_LOGI(TAG, "area %s size: %d", area_str, area_size);

        offset += area_size;
        ESP_LOGI(TAG, "offset %d", offset);

        // check cmd format
        SF_CHECK_EXPR_RETURN(ESP_LOGE, TAG, watering_cmd->data_size != offset, "Wrong SF watering HI *new schecdule* command format");

        // add schedule
        status = sf_watering_add_schdule(schedule_id, start_cron_exp, stop_cron_exp, area_str, area_size, NULL, 0);
        SF_CHECK_EXPECTED_RETURN(ESP_LOGE, TAG, status, SF_FAIL, "Failed to add schedule");

        // update ret — echo back the server-assigned ID
        watering_ret = (sf_watering_hi_cmd_t*)calloc(1, SF_WATERING_HI_CMD_MIN_SIZE + sizeof(uint32_t));
        SF_CHECK_EXPR_RETURN(ESP_LOGE, TAG, watering_ret == NULL, "Failed allocat ret msg to broker");

        watering_ret->cmd = watering_cmd->cmd;
        watering_ret->data_size = sizeof(uint32_t);
        memcpy(&watering_ret->data, &status, watering_ret->data_size);

        break;
    
    case SF_WATERING_REMOVE_SCHEDULER:
        status = SF_FAIL; 
        uint32_t id  = 0;
        
        do
        {
            // check cmd format 
            if ( watering_cmd->data_size != sizeof(uint32_t)) 
            {
                ESP_LOGE(TAG, "SF_WATERING_REMOVE_SCHEDULER: data size bigger than: %d", (int)watering_cmd->data_size);
                break;
            }
        
            id  = ((uint32_t*)watering_cmd->data)[0];

            status = sf_watering_remove_schdule(id);
            SF_CHECK_EXP_NO_RETURN_STATUS(ESP_LOGI, TAG, status != SF_OK, "Removed Schedule ID: 0x%lu end with status: %d", id, status)

        }while(0);


        // update ret for publish
        watering_ret = (sf_watering_hi_cmd_t*)calloc(1, SF_WATERING_HI_CMD_MIN_SIZE + sizeof(uint32_t)); 
        SF_CHECK_EXPR_RETURN(ESP_LOGE, TAG, watering_ret == NULL, "Failed allocate ret msg to broker");

        //watering data will hold status of remove operation
        watering_ret->cmd = watering_cmd->cmd;
        watering_ret->data_size = sizeof(uint32_t);
        memcpy(watering_ret->data, &status, sizeof(uint32_t));
        

        break;
    
    case SF_WATERING_GET_SCHEDULERS:
    {
        ESP_LOGI(TAG, "SF_WATERING_GET_SCHEDULERS");
        uint32_t file_size = 0;

        do
        {
            status = sf_watering_get_file_schedule_list(NULL, &file_size);
            if (status != SF_OK)
            {
                ESP_LOGE(TAG, "Query schedule list size with status: %d", status);
                file_size = sizeof(uint32_t);
            }

            watering_ret = (sf_watering_hi_cmd_t*)calloc(1, SF_WATERING_HI_CMD_MIN_SIZE + file_size);
            SF_CHECK_EXPR_RETURN(ESP_LOGE, TAG, watering_ret == NULL, "Failed allocate ret msg to broker");

            SF_CHECK_ERR_BREAK(ESP_LOGI, TAG, status, "Query schedule list size with status: %d", status);

            status = sf_watering_get_file_schedule_list((uint8_t*)&watering_ret->data, &file_size);
            SF_CHECK_ERR_BREAK(ESP_LOGI, TAG, status, "Read schedule list from file ended with status: %d", status);

            watering_ret->data_size = file_size;

        } while (0);
        
        if (status != SF_OK) 
        {
            watering_ret->data_size = sizeof(uint32_t);
            memcpy(watering_ret->data, &status, sizeof(uint32_t));
        }

        watering_ret->cmd = watering_cmd->cmd;
        break;
    }

    default:
        ESP_LOGE(TAG, "Wrong SF watering command not supported");
        break;
    }

    
    // publish ret to broker 
    status = esp_mqtt_client_publish(sf_watering_mqtt_lient, SF_WATERING_STATUS_TOPIC, (char*)watering_ret, SF_WATERING_HI_CMD_MIN_SIZE + watering_ret->data_size, 1, 0);
    SF_CHECK_EXP_NO_RETURN_STATUS(ESP_LOGI, TAG, status < SF_OK, "MQTT publish retuned %d", status);

    if (watering_ret != NULL)
    {
        free(watering_ret); 
        watering_ret = NULL; 
    }
}

static void sf_watering_hi_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id = 0;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, SF_MQTT_NEW_SCHEDULER_TOPIC, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA_SIZE=%d\r\n", event->data_len);

        sf_watering_hi_cmd_t* cmd_copy = malloc(event->data_len);
        if (cmd_copy == NULL)
        {
            ESP_LOGE(TAG, "MQTT_EVENT_DATA: malloc failed, command dropped");
            break;
        }
        memcpy(cmd_copy, event->data, event->data_len);
        if (xQueueSend(sf_watering_hi_cmd_queue, &cmd_copy, 0) != pdTRUE)
        {
            ESP_LOGE(TAG, "MQTT_EVENT_DATA: queue full, command dropped");
            free(cmd_copy);
        }
        break;
    }
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) 
        {
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

sf_err_t sf_watering_start_host_interface()
{
    sf_err_t status = SF_FAIL; 
    
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_SIM_FLOW_HOST_INTERFACE_URI,
        .broker.address.port = CONFIG_SIM_FLOW_HOST_INTERFACE_PORT,

    };

    sf_watering_hi_cmd_queue = xQueueCreate(SF_WATERING_HI_CMD_QUEUE_DEPTH, sizeof(sf_watering_hi_cmd_t*));
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, sf_watering_hi_cmd_queue, FAIL, "Fail create cmd queue");

    BaseType_t task_ret = xTaskCreate(sf_watering_hi_worker_task, "sf_hi_worker",
                                      SF_WATERING_HI_WORKER_STACK, NULL,
                                      SF_WATERING_HI_WORKER_PRIORITY, NULL);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, task_ret != pdPASS, FAIL, "Fail create worker task");

    sf_watering_mqtt_lient = esp_mqtt_client_init(&mqtt_cfg);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, sf_watering_mqtt_lient, FAIL, "Fail init mqtt client");

    status = esp_mqtt_client_register_event(sf_watering_mqtt_lient, ESP_EVENT_ANY_ID, sf_watering_hi_event_handler, NULL);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Register client handler returned: %d", status);

    status = esp_mqtt_client_start(sf_watering_mqtt_lient);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Start client returned: %d", status);


FAIL:
    return status;
}