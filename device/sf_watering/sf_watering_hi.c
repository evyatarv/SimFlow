#include "sf_watering_hi.h"
#include "sf_watering_scheduler.h"

#include "esp_log.h"
#include "mqtt_client.h"



#define SF_MQTT_NEW_SCHEDULER_TOPIC "/topic/schedule"
#define SF_WATERING_HI_CMD_MIN_SIZE 5


// Tag for logging
static const char* TAG = "SF_WATTERING_HI";

static esp_mqtt_client_handle_t sf_watering_mqtt_lient = NULL; 

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
        int crone_exp_size = watering_schedule[offset];
        int area_size = 0;
        char* start_cron_exp = NULL;
        char* stop_cron_exp = NULL;
        char* area_str = NULL;
        int schedule_id = -1;
                

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
        status = sf_watering_add_schdule(start_cron_exp, stop_cron_exp, area_str, area_size, NULL, 0, &schedule_id);
        ESP_LOGI(TAG, "New Schedule ID: %d", schedule_id);
        SF_CHECK_EXPECTED_RETURN(ESP_LOGE, TAG, status, SF_FAIL, "Failed to add schedule");

        // update ret
        watering_ret = (sf_watering_hi_cmd_t*)calloc(1, SF_WATERING_HI_CMD_MIN_SIZE + sizeof(uint32_t)); 
        SF_CHECK_EXPR_RETURN(ESP_LOGE, TAG, watering_ret == NULL, "Failed allocat ret msg to broker");

        watering_ret->cmd = watering_cmd->cmd;
        watering_ret->data_size = sizeof(uint32_t);
        memcpy(watering_ret->data, &schedule_id, watering_ret->data_size);

        break;
    
    case SF_WATERING_REMOVE_SCHEDULER:
        sf_err_t status = SF_FAIL; 
        if ( watering_cmd->data_size != sizeof(int) ) 
        {
            ESP_LOGE(TAG, "SF_WATERING_REMOVE_SCHEDULER: data size bigger than int =%d", (int)watering_cmd->data_size);
            return;
        }
       int id  = (int) *(watering_cmd->data);
       status = sf_watering_remove_schdule(id);
       SF_CHECK_EXPECTED_RETURN(ESP_LOGE, TAG, status, SF_FAIL, "Failed to remove schedule 0x%x",id);
       ESP_LOGI(TAG, "Removed Schedule ID: 0x%x", id);

        // update ret for publish
        watering_ret = (sf_watering_hi_cmd_t*)calloc(1, SF_WATERING_HI_CMD_MIN_SIZE + sizeof(uint32_t)); 
        SF_CHECK_EXPR_RETURN(ESP_LOGE, TAG, watering_ret == NULL, "Failed allocate ret msg to broker");

        //watering data will hold status of remove operation
        watering_ret->cmd = watering_cmd->cmd;
        watering_ret->data_size = sizeof(uint32_t);
        memcpy(watering_ret->data, &status, sizeof(uint32_t));
        

        break;
    
    case SF_WATERING_GET_SCHEDULERS:
        /* code */
        ESP_LOGI(TAG, "SF_WATERING_GET_SCHEDULERS "); 
        break;


    default:
        ESP_LOGE(TAG, "Wrong SF watering command not supported");
        break;
    }

    
    // publish ret to broker 
    status = esp_mqtt_client_publish(sf_watering_mqtt_lient, SF_WATERING_STATUS_TOPIC, (char*)watering_ret, SF_WATERING_HI_CMD_MIN_SIZE + watering_ret->data_size, 1, 0);
    SF_CHECK_ERR_NO_RETURN_STATUS(ESP_LOGI, TAG, status, "Failed to publish ");

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
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA_SIZE=%d\r\n", event->data_len);
        //ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);

        sf_watering_hi_cmd_parser(event->data, event->data_len);
        
        break;
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

    sf_watering_mqtt_lient = esp_mqtt_client_init(&mqtt_cfg);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, sf_watering_mqtt_lient, FAIL, "Fail init mqtt client");

    status = esp_mqtt_client_register_event(sf_watering_mqtt_lient, ESP_EVENT_ANY_ID, sf_watering_hi_event_handler, NULL);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Register client handler returned: %d", status);

    status = esp_mqtt_client_start(sf_watering_mqtt_lient);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Start client returned: %d", status);


FAIL:
    return status;
}