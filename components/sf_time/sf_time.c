#include "sf_time.h"
#include "esp_log.h"

#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"
#include <sys/time.h>



static const char* TAG = "SF_TIME";


void sf_time_print_current_time()
{
    time_t now = 0;
    time(&now);
    ESP_LOGI(TAG,"Device Time -------> : %s", ctime(&now));
}


sf_err_t sf_time_init()
{
    esp_err_t status = ESP_FAIL;

    ESP_LOGI(TAG, "Initializing date ...");
    
    status = sf_time_set_sntp_date();

    sf_time_print_current_time();

    return status; 
}


sf_err_t sf_time_set_sntp_date()
{
    sf_err_t status = SF_FAIL;
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SIM_FLOW_SNTP_SERVER);
    config.smooth_sync = true; // Enable smooth sync for better time accuracy

    ESP_LOGI(TAG, "Initializing SNTP ...");

    esp_netif_sntp_init(&config);

    int retry = 1;
    while (retry <= CONFIG_SIM_FLOW_SNTP_RETRY_COUNT) {

        if (esp_netif_sntp_sync_wait(CONFIG_SIM_FLOW_SNTP_TIMEOUT / portTICK_PERIOD_MS) != ESP_ERR_TIMEOUT)
        {
            status = SF_OK;
            break;
        }

        ESP_LOGI(TAG, "Waiting for system time to be set ... (%d/%d)", retry, CONFIG_SIM_FLOW_SNTP_RETRY_COUNT);

        retry++;
    }

    if (retry > CONFIG_SIM_FLOW_SNTP_RETRY_COUNT) {
        ESP_LOGW(TAG, "Failed to set system time via SNTP");
        status = SF_FAIL;
    }

    sf_time_print_current_time();

    return status;
}