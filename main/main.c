#include <stdio.h>
#include "sf_device.h"
#include "esp_log.h"


static const char* TAG = "SIMFLOW_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "************************ SIM FLOW ************************");
    sf_device_cfg_t dev_cfg = {0}; 

    init_device(dev_cfg);
    ESP_LOGI(TAG, "************************ SIM FLOW INIT DONE STARTING DEVICE ************************");

    device_start();
}