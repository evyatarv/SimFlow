#include "esp_wifi.h"
#include "esp_log.h"

#include "sf_wifi.h"
#include "freertos/event_groups.h"

static int s_retry_num = 0;
static const char* TAG = "SF_WIFI";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < SF_WIFI_CONFIG_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Connect to the AP fail, retry to connect to the AP ...");
        } 
        else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected to AP, Device IP:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

sf_err_t sf_wifi_init(void)
{
    esp_err_t status = ESP_FAIL;

    s_wifi_event_group = xEventGroupCreate();

    status = esp_netif_init();
    SF_CHECK_ERR_GOTO(ESP_LOGV, TAG, status, FAIL, "esp_netif_init status: %d", status);

    esp_event_loop_create_default();
    SF_CHECK_ERR_GOTO(ESP_LOGV, TAG, status, FAIL, "esp_event_loop_create_default status: %d", status);

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    SF_CHECK_ERR_GOTO(ESP_LOGV, TAG, status, FAIL, "esp_wifi_init status: %d", status);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    status = esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id);
    SF_CHECK_ERR_GOTO(ESP_LOGV, TAG, status, FAIL, "instance_any_id - Register event handler instance status: %d", status);

    status = esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip);
    SF_CHECK_ERR_GOTO(ESP_LOGV, TAG, status, FAIL, "instance_got_ip - Register event handler instance status: %d", status);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_SIM_FLOW_WIFI_SSID,
            .password = CONFIG_SIM_FLOW_WIFI_PASSWORD,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    status = esp_wifi_set_mode(WIFI_MODE_STA);
    SF_CHECK_ERR_GOTO(ESP_LOGV, TAG, status, FAIL, "Set mode status: %d", status);

    status = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    SF_CHECK_ERR_GOTO(ESP_LOGV, TAG, status, FAIL, "Set config status: %d", status);

    status = esp_wifi_start() ;
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Start wifi status: %d", status);

        /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);
    
    if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", wifi_config.sta.ssid);
        goto FAIL;
    } else if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID:%s", wifi_config.sta.ssid);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        goto FAIL;
    }

    ESP_LOGI(TAG, "wifi_init_sta finished succesfully.");
    return SF_OK;

FAIL:
    ESP_LOGI(TAG, "wifi_init_sta failed.");
    return SF_FAIL;
}

sf_err_t sf_wifi_stop(void)
{
    esp_err_t status = ESP_FAIL; 

    status = esp_wifi_stop();
    SF_CHECK_ERR_RETURN_FAIL(ESP_LOGI, TAG, status, "Stop wifi status: %d", status);

    ESP_LOGI(TAG, "wifi_stopped.");
    return SF_OK;
}