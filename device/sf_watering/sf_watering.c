#include "sf_device.h"
#include "sf_gpio.h"
#include "sf_wifi.h"
#include "sf_time.h"
#include "sf_watering_scheduler.h"
#include "sf_watering_hi.h"
#include "cron.h"
#include "sf_files.h"
#include <errno.h>

#include <sys/time.h>

#include "nvs_flash.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_vfs.h"

static const char* TAG = "SF_WATTERING";

typedef struct sf_device_sts
{
    union {
        struct {
            unsigned int device_init                : 1; // device initialized
            unsigned int device_wifi_sts            : 1; // device wifi status
            unsigned int device_fs_sts              : 1; // device fs status
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

    if (sf_file_init_fs("/sf_fatfs"))
        goto FAIL;
    g_device_sts.device_fs_sts = 0x1; // device fs initialized

    g_device_sts.device_init = 0x1; // device initialized

    return SF_OK; 

FAIL:
    return SF_FAIL; 
}


sf_err_t sf_watering_device_start ()
{
    ESP_LOGI(TAG, "Starting watering device ....");

    char line[128];

    FILE *f;
    char *pos;
    ESP_LOGI(TAG, "Reading file");

    const char *host_filename1 = "/sf_fatfs/sf_watering/sfHelloFile.txt";

    struct stat info;

    if(stat(host_filename1, &info) < 0){
        ESP_LOGE(TAG, "Failed to read file stats %d", errno);
        return 1;
    }

    f = fopen(host_filename1, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return 1;
    }
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    // Unmount FATFS
    ESP_LOGI(TAG, "Unmounting FAT filesystem");
    sf_file_deinit_fs("/sf_fatfs");

    sf_watering_start_host_interface();
    


    return SF_OK;
}

