#include "sf_device.h"
#include "sf_gpio.h"
#include "sf_timer.h"
#include "sf_wifi.h"

#include <sys/time.h>

#include "nvs_flash.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "driver/gptimer_types.h"


static const char* TAG = "SF_DEVICE";

typedef struct sf_device_sts_t
{
    union {
        struct {
            unsigned int device_init                : 1; // device initialized
            unsigned int device_gpio_pin_0_sts      : 1; // device GPIO pin status
            unsigned int device_wifi_sts            : 1; // device wifi status
            unsigned int reserved: 31;
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

static bool IRAM_ATTR timer_on_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    device_sts.device_gpio_pin_0_sts = !device_sts.device_gpio_pin_0_sts; // toggle GPIO status

    sf_gpio_set_level(CONFIG_GPIO_OUTPUT_0, device_sts.device_gpio_pin_0_sts); // set GPIO level
    

    return pdTRUE; 
}


sf_err_t init_device(sf_device_cfg_t dev_cfg)
{

    if (sf_gpio_init())
        goto FAIL;

    if(sf_timer_init(&timer_on_alarm_cb, SF_TIMER_RESOLUTION_MICRO_S))
        goto FAIL;

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    if (sf_wifi_init())
        goto FAIL;  
    device_sts.device_wifi_sts = 0x1; // device wifi initialized


    device_sts.device_init = 0x1; // device initialized

    return SF_OK; 

FAIL:
    return SF_FAIL; 
}



sf_err_t device_start ()
{


    // GPIO and timer 
/*
    int gpio_val = 0;

    sf_timer_start(SF_TIMER_RESOLUTION_MICRO_S); 

    int counter = 10;
    while(counter){
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_val = sf_gpio_get_level(CONFIG_GPIO_OUTPUT_0);

        ESP_LOGI(TAG, "Device is running GPIO level: %d and not: %d", gpio_val, !gpio_val);

        counter--;
    }

    sf_timer_stop();
*/
    // time 
   // settimeofday();
    
   time_t t;
	time_t t_new;
	double difference = 0.0;

	time(&t);	

	printf("Time: %s", ctime(&t));
	
	struct tm *ptm = localtime(&t);


	for(int i=0 ; i<365; i++)
	{
		ptm->tm_mday++;

		t_new = mktime(ptm);
	    printf("Time: %s", ctime(&t_new));
        difference = difftime(t_new, t);
        printf ("Difference is %.0f seconds\n", difference);
	}

    return SF_OK;
}

