#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "sf_device.h"
#include "sf_err.h"

#include "sf_locator.h"
#include "sf_locator_led.h"
#include "sf_locator_mpu.h"
#include "sf_locator_button.h"
#include "sf_locator_telegram.h"
#include "sf_locator_web.h"
#include "config.h"

#define WIFI_TEST_INTERVAL_MS       15000
#define WIFI_RECONNECT_INTERVAL_MS  60000

#define WIFI_STA_GOT_IP_BIT     BIT0
#define WIFI_STA_FAIL_BIT       BIT1

static const char *TAG = "SF_LOCATOR";

static EventGroupHandle_t   s_wifi_events = NULL;
static bool                 s_sta_connected = false;
static bool                 s_was_connected = false;

static uint32_t now_ms_from_ticks(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
    if (base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            s_sta_connected = false;
            xEventGroupSetBits(s_wifi_events, WIFI_STA_FAIL_BIT);
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "AP: station connected");
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "AP: station disconnected");
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&evt->ip_info.ip));
        s_sta_connected = true;
        s_was_connected = true;
        xEventGroupSetBits(s_wifi_events, WIFI_STA_GOT_IP_BIT);
    }
}

static sf_err_t wifi_init_apsta(void)
{
    sf_err_t status = SF_FAIL;
    esp_err_t err;

    err = esp_netif_init();
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end, "esp_netif_init with status: %d", err);

    err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE) {
        err = ESP_OK;
    }
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end,
                      "esp_event_loop_create_default with status: %d", err);

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end, "esp_wifi_init with status: %d", err);

    s_wifi_events = xEventGroupCreate();
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, s_wifi_events, end,
                       "xEventGroupCreate returned NULL");

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              &wifi_event_handler, NULL, NULL);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end,
                      "register WIFI_EVENT handler with status: %d", err);

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              &wifi_event_handler, NULL, NULL);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end,
                      "register IP_EVENT handler with status: %d", err);

    wifi_config_t sta_cfg = { 0 };
    strncpy((char *)sta_cfg.sta.ssid, WIFI_SSID, sizeof(sta_cfg.sta.ssid) - 1);
    strncpy((char *)sta_cfg.sta.password, WIFI_PASSWORD, sizeof(sta_cfg.sta.password) - 1);
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end,
                      "esp_wifi_set_mode APSTA with status: %d", err);

    err = esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end,
                      "esp_wifi_set_config STA with status: %d", err);

    err = esp_wifi_start();
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end, "esp_wifi_start with status: %d", err);

    /* Wait briefly for STA to connect, but do not fail the device on timeout
     * (Soft-AP/web UI will still come up). */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_events,
            WIFI_STA_GOT_IP_BIT | WIFI_STA_FAIL_BIT,
            pdFALSE, pdFALSE, pdMS_TO_TICKS(20000));
    if (bits & WIFI_STA_GOT_IP_BIT) {
        ESP_LOGI(TAG, "WiFi STA connected to '%s'", WIFI_SSID);
    } else {
        ESP_LOGW(TAG, "WiFi STA did not connect within timeout - continuing");
    }

    status = SF_OK;
end:
    return status;
}

static void wifi_watchdog(uint32_t now_ms)
{
    static uint32_t s_last_test_ms = 0;
    static uint32_t s_last_retry_ms = 0;

    if ((now_ms - s_last_test_ms) <= WIFI_TEST_INTERVAL_MS) {
        return;
    }
    s_last_test_ms = now_ms;

    if (s_sta_connected) {
        ESP_LOGD(TAG, "WiFi OK");
        return;
    }

    ESP_LOGI(TAG, "WiFi down - status check");
    if ((now_ms - s_last_retry_ms) > WIFI_RECONNECT_INTERVAL_MS) {
        s_last_retry_ms = now_ms;
        ESP_LOGI(TAG, "WiFi retry connect()");
        esp_wifi_disconnect();
        esp_wifi_connect();
    }
}

static void handle_button(uint32_t now_ms)
{
    bool triple_mode = sf_locator_led_is_wifi_lost();
    sf_button_event_t event = sf_locator_button_tick(now_ms, triple_mode);

    if (event == SF_BUTTON_EVENT_NONE) {
        return;
    }

    if (triple_mode) {
        if (event == SF_BUTTON_EVENT_TRIPLE_COMPLETE) {
            sf_locator_led_set_wifi_lost(false);
            s_was_connected = false;
            ESP_LOGI(TAG, "WiFi lost alert dismissed");
        }
        return;
    }

    /* LED OFF mode: ignore presses (calibration is init-only). */
    if (sf_locator_led_get_color() == SF_LED_COLOR_OFF &&
        !sf_locator_led_get_raw_active()) {
        ESP_LOGI(TAG, "LED off - press ignored");
        return;
    }

    if (event == SF_BUTTON_EVENT_DOUBLE) {
        /* Telegram chip-temp readout. ESP32-S3 internal temp via temp sensor
         * driver is non-trivial here; send a placeholder so the action still
         * has visible effect. */
        sf_locator_telegram_send("Double-click: chip temperature N/A");
    } else if (event == SF_BUTTON_EVENT_SINGLE) {
        sf_led_color_t c = sf_locator_led_cycle();
        char msg[64];
        if (c == SF_LED_COLOR_OFF) {
            snprintf(msg, sizeof(msg), "LED turned off");
        } else {
            snprintf(msg, sizeof(msg), "Color changed to %s",
                     sf_locator_led_color_name(c));
        }
        ESP_LOGI(TAG, "%s", msg);
        sf_locator_telegram_send(msg);
    }
}

static void locator_loop_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t now_ms = now_ms_from_ticks();

        sf_locator_web_tick(now_ms);
        sf_locator_mpu_tick(now_ms);

        /* Detect transitions for WiFi-lost alert mode. */
        if (s_was_connected && !s_sta_connected) {
            sf_locator_led_set_wifi_lost(true);
        }

        wifi_watchdog(now_ms);
        handle_button(now_ms);
        sf_locator_led_tick(now_ms);

        vTaskDelay(pdMS_TO_TICKS(SF_LOCATOR_LOOP_PERIOD_MS));
    }
}

sf_err_t init_device(sf_device_cfg_t dev_cfg)
{
    sf_err_t status = SF_FAIL;
    esp_err_t err;
    (void)dev_cfg;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end, "nvs_flash_init with status: %d", err);

    if (sf_locator_led_init() != SF_OK) {
        ESP_LOGW(TAG, "LED init failed - continuing");
    }
    if (sf_locator_button_init() != SF_OK) {
        ESP_LOGW(TAG, "Button init failed - continuing");
    }
    if (sf_locator_mpu_init() != SF_OK) {
        ESP_LOGW(TAG, "MPU init failed - lift detection disabled");
    }

    if (wifi_init_apsta() != SF_OK) {
        ESP_LOGE(TAG, "WiFi APSTA init failed");
        goto end;
    }

    if (sf_locator_web_init() != SF_OK) {
        ESP_LOGW(TAG, "Web/SoftAP init failed - continuing");
    }

    status = SF_OK;
end:
    return status;
}

sf_err_t device_start(void)
{
    sf_err_t status = SF_FAIL;
    BaseType_t ok;

    ESP_LOGI(TAG, "Starting locator device ...");

    sf_locator_telegram_send("simpleBot started! Blinking red.");

    ok = xTaskCreate(locator_loop_task, "locator_loop", 6144, NULL, 5, NULL);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, ok != pdPASS, end,
                       "xTaskCreate locator_loop with status: %d", ok);

    status = SF_OK;
end:
    return status;
}
