
#include "sf_pm.h"
#include "esp_log.h"
#include "esp_pm.h"

static const char* TAG = "SF_PWR_MGR";

sf_err_t sf_pm_set_light_sleep_power_mode(bool enable)
{
    // Configure dynamic power management and automatic light sleep
#if CONFIG_PM_ENABLE

    sf_err_t status = SF_FAIL;

    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_XTAL_FREQ, // Drop to XTAL frequency when idle
        .light_sleep_enable = enable
    };

    status = esp_pm_configure(&pm_config);
    SF_CHECK_ERR_RETURN_STATUS(ESP_LOGI, TAG, status, "esp_pm_configure status: %d", status);
#else
    ESP_LOGW(TAG, "esp pm not enabled");
    return SF_OK;
#endif

return SF_OK;
}