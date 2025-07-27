#include "sf_device.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"

#include "include/sf_watering.h"





static const char* TAG = "SF_DEVICE";

typedef struct sf_device_sts_t
{
    union {
        struct {
            unsigned int device_init                : 1; // device initialized
            unsigned int device_gpio_pin_0_sts      : 1; // device GPIO pin status
            unsigned int device_wifi_sts            : 1; // device wifi status
            unsigned int reserved: 29;
        };
        unsigned int dev_cfg;
    };
} sf_device_sts_t;

static sf_device_sts_t device_sts = {0};


static bool check_device_sts()
{
    if (device_sts.device_init == 0x1) {
        return true;
    } else {
        return false;
    }
}

sf_err_t init_device(sf_device_cfg_t dev_cfg)
{
    if(!sf_watering_device_init())
        goto FAIL; 

    return SF_OK; 

FAIL:
    return SF_FAIL; 
}



sf_err_t device_start ()
{	sf_watering_device_start();
    return SF_OK;
}

