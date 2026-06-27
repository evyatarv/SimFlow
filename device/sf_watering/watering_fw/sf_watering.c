#include "sf_device.h"
#include "sf_gpio.h"
#include "sf_time.h"
#include "sf_pm.h"

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

#ifdef CONFIG_SIM_FLOW_EMULATION_BUILD
#include "sf_eth.h"
#else
#include "sf_wifi_prov.h"
#define WEBUI_DIR "/sf_fatfs/sf_watering"
#endif



static const char* TAG = "SF_WATTERING";

/* A running provisioning SoftAP cannot light-sleep, so this gates the power
 * mode (DESIGN §13). Updated by on_prov_state; defaults false (emulation
 * build and the connected boot path never enter provisioning). */
static bool s_provisioning = false;

#ifndef CONFIG_SIM_FLOW_EMULATION_BUILD
/* Fired by sf_wifi_prov on SoftAP up/down. Keeps light-sleep disabled while
 * the SoftAP is up. LED indication is wired in a later commit. */
static void on_prov_state(sf_wifi_prov_state_t state, void *ctx)
{
    s_provisioning = (state == SF_WIFI_PROV_PROVISIONING);
    sf_pm_set_light_sleep_power_mode(!s_provisioning);
}

/* Strings must outlive the component — all static storage (DESIGN §5.3). */
static const sf_wifi_prov_config_t prov_cfg = {
    .ap_ssid           = "SIMFLOW-Watering",
    .prov_html_path    = WEBUI_DIR "/prov.html",
    .success_html_path = WEBUI_DIR "/success.html",
    .logo_path         = WEBUI_DIR "/logo.png",
    .on_state_change   = on_prov_state,
    .state_ctx         = NULL,
};
#endif

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

static sf_err_t init_network(void)
{
#ifdef CONFIG_SIM_FLOW_EMULATION_BUILD
    return sf_eth_init();
#else
    return sf_wifi_prov_init(&prov_cfg);
#endif
}

sf_err_t init_device(sf_device_cfg_t dev_cfg)
{
    if (sf_gpio_init())
        goto FAIL;

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    /* FS + schedules first: the watering core must run offline, and the
     * provisioning UI files must be mounted before the SoftAP can come up. */
    if (sf_file_init_fs("/sf_fatfs"))
        goto FAIL;
    g_device_sts.device_fs_sts = 0x1; // device fs initialized

    sf_watering_load_from_file(SF_WATERING_SCHEDULE_FILE);

    /* TZ before network: a POST /time during provisioning converts the naive
     * datetime via mktime() in the device timezone (DESIGN §9.2). */
    sf_time_set_timezone(NULL);

    /* Network: blocks through the bounded initial connect, then returns. May
     * bring up the provisioning SoftAP, which fires on_prov_state(). */
    if (init_network())
        goto FAIL;
    g_device_sts.device_wifi_sts = 0x1; // device network initialized

    /* Time: must follow init_network (needs esp_netif_init). SNTP failure is
     * NOT fatal — the scheduler gates on sf_time_is_valid(), and time can also
     * be set manually via the provisioning page. */
    if (sf_time_init())
        ESP_LOGW(TAG, "sf_time_init failed — continuing without SNTP");
    sf_time_sntp_restart();

    /* Initial power mode: light-sleep enabled unless the SoftAP came up during
     * init_network(); on_prov_state() keeps it in sync thereafter. */
    if (sf_pm_set_light_sleep_power_mode(!s_provisioning))
        goto FAIL;

    /* WDT note (DESIGN §12): app_main is not subscribed to the Task WDT and is
     * deleted after device_start() returns, so the ~25-30 s blocking connect in
     * init_network() cannot trip it. If a long-lived task is ever subscribed,
     * call esp_task_wdt_add() HERE — after the blocking connect, never before. */

    g_device_sts.device_init = 0x1; // device initialized

    return SF_OK;

FAIL:
    return SF_FAIL;
}



sf_err_t device_start ()
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
        return SF_FAIL;
    }

    f = fopen(host_filename1, "rb");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return SF_FAIL;
    }
    fgets(line, sizeof(line), f);
    fclose(f);
    // strip newline
    pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    sf_watering_start_host_interface();

    return SF_OK;
}
