
#include "sf_gpio.h"
#include "sf_gpio_cfg.h"
#include "esp_log.h"



static const char* TAG = "SF_GPIO";

sf_err_t sf_gpio_init()
{
    esp_err_t status = ESP_FAIL;

    //zero-initialize the config structure.
    gpio_config_t io_conf = {0};


    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;

    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;

    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    
    ESP_LOGV(TAG, "GPIO cnf: intr_type->%d, mode->%d, mask->0x%llx",io_conf.intr_type, io_conf.mode, io_conf.pin_bit_mask);
    //configure GPIO with the given settings
    status = gpio_config(&io_conf);
    ESP_LOGI(TAG, "GPIO init stats = %d", status);

    if (status)
        return SF_FAIL;

    ESP_LOGI(TAG, "************************ GPIO init DONE ************************");
    return SF_OK;
}

void sf_gpio_set_level(uint32_t gpio_num, uint8_t level)
{
    esp_err_t status = ESP_FAIL;
    
    status = gpio_set_level(gpio_num, level);

    ESP_LOGI(TAG, "GPIO set level to %u stats = %d", level, status);
}

int sf_gpio_get_level(uint32_t gpio_num)
{
    int val = 0;
    
    val = gpio_get_level(gpio_num);
    ESP_LOGI(TAG, "GPIO get level val = %d", val);

    return (val);
}