#include "driver/gptimer.h"
#include "sf_timer.h"
#include "esp_log.h"

static const char* TAG = "SF_TIMER";

static gptimer_handle_t gptimer = NULL;

sf_err_t sf_timer_init(gptimer_alarm_cb_t cb, uint32_t timer_resolution)
{
    esp_err_t status = ESP_OK; 

    if (cb == NULL)
    {
        status = SF_ERR_TIMER_INVAL_PARAM;
        SF_CHECK_ERR_RETURN(ESP_LOGE, TAG, status, "CB can't be null")
    }

    ESP_LOGI(TAG, "Create timer handle");
    
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = timer_resolution, 
    };

    status = gptimer_new_timer(&timer_config, &gptimer);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Create timer handle status: %d", status);

    ESP_LOGI(TAG, "Register alarm callback");
    gptimer_event_callbacks_t cbs = {
        .on_alarm = *cb,
    };

    status = gptimer_register_event_callbacks(gptimer, &cbs, NULL);
    //SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Registar handle cb status: %d", status);
    
    ESP_LOGI(TAG, "Enable timer");
    status = gptimer_enable(gptimer);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Enable timer status: %d", status);
   
    ESP_LOGI(TAG, "************************ TIMERS init DONE ************************");

    return SF_OK;

FAIL:
    return status;
}


void sf_timer_start(uint64_t count)
{
    esp_err_t status = ESP_FAIL; 

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = count,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    
    status = gptimer_set_alarm_action(gptimer, &alarm_config);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Set alarm status: %d", status);

    status = gptimer_start(gptimer);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Timer start status: %d", status);

    ESP_LOGI(TAG, "Starting timer ...");
    FAIL:
        return; 
}

void sf_timer_stop()
{
    esp_err_t status = ESP_FAIL;

    ESP_LOGI(TAG, "Stop timer");
    status = gptimer_stop(gptimer);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Timer stop status: %d", status);
    
    ESP_LOGI(TAG, "Disable timer");
    status = gptimer_disable(gptimer);
    SF_CHECK_ERR_GOTO(ESP_LOGI, TAG, status, FAIL, "Timer disable status: %d", status);

    FAIL:
        return; 
} 

void sf_timer_clear()
{

}