#include <time.h>
#include <sys/time.h>

#include "esp_attr.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"

#include "sf_time.h"

#define SF_TIME_RTC_MAGIC  0x5F54494Du  /* "_TIM" */

ESP_EVENT_DEFINE_BASE(SF_TIME_EVENT);

static const char *TAG = "SF_TIME";

RTC_NOINIT_ATTR static uint32_t s_rtc_magic;
RTC_NOINIT_ATTR static bool     s_rtc_time_valid;

/* ------------------------------------------------------------------ */

static void sf_time_mark_valid(void)
{
    /* No mutex: concurrent SNTP + manual-set can both pass !s_rtc_time_valid
     * and post SF_TIME_EVENT_VALID twice. Benign — all handlers are idempotent
     * LED/state re-evals that tolerate a duplicate event. */
    if (!s_rtc_time_valid) {
        s_rtc_time_valid = true;
        if (esp_event_post(SF_TIME_EVENT, SF_TIME_EVENT_VALID,
                           NULL, 0, pdMS_TO_TICKS(100)) != ESP_OK) {
            ESP_LOGW(TAG, "post SF_TIME_EVENT_VALID failed");
        }
    }
}

static void sf_time_rtc_guard(void)
{
    if (s_rtc_magic != SF_TIME_RTC_MAGIC) {
        /* RTC domain was unpowered — contents are garbage, reset both fields. */
        s_rtc_magic      = SF_TIME_RTC_MAGIC;
        s_rtc_time_valid = false;
    }
    /* else: magic intact → s_rtc_time_valid carried over from before the reset */
}

static void sntp_sync_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP sync OK");
    sf_time_print_current_time();
    sf_time_mark_valid();
}

/* ------------------------------------------------------------------ */

sf_err_t sf_time_init(void)
{
    sf_time_rtc_guard();

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SIM_FLOW_SNTP_SERVER);
    config.sync_cb    = sntp_sync_cb;
    config.start      = false;  /* started on demand via sf_time_sntp_restart() */
    /* smooth_sync intentionally left false: a step-change is faster to a valid
     * clock on first sync, which matters more than slew accuracy for a scheduler. */

    esp_err_t status = esp_netif_sntp_init(&config);
    SF_CHECK_ERR_RETURN_FAIL(ESP_LOGE, TAG, status, "esp_netif_sntp_init: %d", status);

    return SF_OK;
}

bool sf_time_is_valid(void)
{
    return s_rtc_time_valid;
}

sf_err_t sf_time_set_manual(time_t epoch)
{
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    int status = settimeofday(&tv, NULL);
    SF_CHECK_ERR_RETURN_FAIL(ESP_LOGE, TAG, status, "settimeofday failed: %d", status);
    ESP_LOGI(TAG, "Time set manually");
    sf_time_mark_valid();
    return SF_OK;
}

sf_err_t sf_time_sntp_restart(void)
{
    if (s_rtc_time_valid) {
        return SF_OK;
    }
    esp_netif_sntp_start();  /* no-op if already running */
    sntp_restart();          /* force immediate resync attempt */
    return SF_OK;
}

sf_err_t sf_time_set_timezone(const char *timezone)
{
    const char *tz = (timezone != NULL) ? timezone : CONFIG_SIM_FLOW_TIME_DEFUALT_TIMEZONE;
    int status = setenv("TZ", tz, 1);
    SF_CHECK_ERR_RETURN_STATUS(ESP_LOGI, TAG, status, "Set timezone: %d", status);
    tzset();
    return SF_OK;
}

void sf_time_print_current_time(void)
{
    time_t now = 0;
    time(&now);
    ESP_LOGI(TAG, "Device Time: %s", ctime(&now));
}

char *sf_time_get_current_time(void)
{
    time_t now = 0;
    time(&now);
    return ctime(&now);
}

/* Deprecated — removed in commit 5 when sf_watering switches to
 * sf_time_init() + sf_time_sntp_restart(). */
sf_err_t sf_time_set_sntp_date(void)
{
    if (sf_time_init() != SF_OK) {
        return SF_FAIL;
    }
    return sf_time_sntp_restart();
}
