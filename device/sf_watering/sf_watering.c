#include "sf_device.h"
#include "sf_gpio.h"
#include "sf_wifi.h"
#include "sf_time.h"
#include "sf_watering_scheduler.h"
#include "cron.h"


#include <sys/time.h>

#include "nvs_flash.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include "driver/gptimer_types.h"


static const char* TAG = "SF_WATTERING";

typedef struct sf_device_sts_t
{
    union {
        struct {
            unsigned int device_init                : 1; // device initialized
            unsigned int device_wifi_sts            : 1; // device wifi status
            unsigned int reserved: 29;
        };
        unsigned int dev_cfg;
    };
} sf_device_sts_t;

sf_device_sts_t g_device_sts = {0};


sf_err_t sf_watering_device_init()
{
    if (sf_gpio_init())
        goto FAIL;

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    if (sf_wifi_init())
        goto FAIL;  
    g_device_sts.device_wifi_sts = 0x1; // device wifi initialized

    sf_time_set_timezone(NULL); 

    if(sf_time_set_sntp_date())
        goto FAIL;

    

    g_device_sts.device_init = 0x1; // device initialized

    return SF_OK; 

FAIL:
    return SF_FAIL; 
}


sf_err_t sf_watering_device_start ()
{
    ESP_LOGI(TAG, "Starting watering device ...");

    time_t t;
    time_t t_new;
    double difference = 0.0;

    time(&t);	

    printf("Time: %s", ctime(&t));

    struct tm *ptm = localtime(&t);

    uint32_t pin = CONFIG_GPIO_OUTPUT_0; 

    sf_watering_add_schdule("0 15 18 * * SUN", "0 16 18 * * SUN", "Trees", 5, &pin, SF_WATERING_USER_DATA_SIZE);
    cron_start();
    vTaskDelay((2000 * 1000) / portTICK_PERIOD_MS); // This is just to emulate a delay between the calls
    cron_stop();
    cron_job_clear_all();

    return SF_OK;
}

