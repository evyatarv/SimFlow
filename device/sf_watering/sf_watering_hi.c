#include "sf_watering_hi.h"
#include "sf_watering_scheduler.h"

#include "esp_log.h"
#include "mqtt_client.h"


#define SF_MQTT_NEW_SCHEDULER_TOPIC "/topic/schedule"
#define SF_WATERING_HI_CMD_MIN_SIZE 5

// Tag for logging
static const char* TAG = "SF_WATTERING_HI";

static esp_mqtt_client_handle_t client = NULL; 

typedef struct  sf_watering_hi_cmd
{
    uint8_t     cmd;
    uint32_t    data_size;
    void*       data;
}
sf_watering_hi_cmd_t;

enum SF_SCHEDULER_CMD
{
    SF_WATERING_NEW_CHEDULER = 1, 
    SF_WATERING_REMOVE_SCHEDULER, 
    SF_WATERING_GET_SCHEDULERS
};


static void sf_watering_hi_cmd_parser(void* cmd, size_t data_size)
{
    sf_watering_hi_cmd_t* new_cmd = NULL; 

    if (cmd == NULL || data_size < SF_WATERING_HI_CMD_MIN_SIZE)
    {
        ESP_LOGE(TAG, "Wrong SF watering HI command format");
        
        goto FAIL;
    }

    new_cmd = (sf_watering_hi_cmd_t*)cmd;

    switch (new_cmd->cmd)
    {
    case SF_WATERING_NEW_CHEDULER:
        /* code */
        break;
    
    case SF_WATERING_REMOVE_SCHEDULER:
        /* code */
        break;
    
    case SF_WATERING_GET_SCHEDULERS:
        /* code */
        break;


    default:
        ESP_LOGE(TAG, "Wrong SF watering command not supported");
        break;
    }





FAIL:
    return;

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
        ESP_LOGD(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
        ESP_LOGD(TAG, "DATA_SIZE=%d\r\n", event->data_len);
        ESP_LOGD(TAG, "DATA=%.*s\r\n", event->data_len, event->data);

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

    client = esp_mqtt_client_init(&mqtt_cfg);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, client, FAIL, "Fail init mqtt client");

    status = esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, sf_watering_hi_event_handler, NULL);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Register client handler returned: %d", status);

    status = esp_mqtt_client_start(client);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Start client returned: %d", status);


FAIL:
    return status;
}