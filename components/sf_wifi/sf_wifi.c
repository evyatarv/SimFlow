#include <string.h>
#include <inttypes.h>

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "sf_wifi.h"

#define SF_WIFI_MAXIMUM_RETRY               5
#define SF_WIFI_RECONNECT_DELAY_INIT_MS     1000
#define SF_WIFI_RECONNECT_DELAY_MAX_MS      30000
#define SF_WIFI_PERPETUAL_AUTH_FAIL_MAX     3

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_AUTH_FAIL_BIT  BIT1
#define WIFI_NO_AP_BIT      BIT2

ESP_EVENT_DEFINE_BASE(SF_WIFI_EVENT);

static const char *TAG = "SF_WIFI";

static bool                  s_driver_initialized     = false; /* TODO(commit5): remove once sf_wifi_init() is gone */
static volatile bool         s_initial_connect_phase  = false;
static uint32_t              s_reconnect_delay_ms     = SF_WIFI_RECONNECT_DELAY_INIT_MS;

static EventGroupHandle_t    s_wifi_event_group       = NULL;
static esp_netif_t          *s_ap_netif               = NULL;
static esp_timer_handle_t    s_reconnect_timer        = NULL;
static int                   s_retry_num              = 0;
static int                   s_auth_fail_count        = 0;

/* Serializes the compound radio operations (mode/config/start/connect) that
 * are issued from different tasks: reconnect_timer_cb on the esp_timer task vs
 * sf_wifi_ap_start/ap_stop on the event-loop task. Without it, ap_start's
 * get_mode→set_mode read-modify-write can race a reconnect's esp_wifi_connect()
 * and act on a stale mode. Never held across the blocking wait in
 * sf_wifi_connect(). */
static SemaphoreHandle_t     s_wifi_lock              = NULL;

/* ------------------------------------------------------------------ */

static void reconnect_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "Reconnect attempt");
    xSemaphoreTake(s_wifi_lock, portMAX_DELAY);
    esp_wifi_connect();
    xSemaphoreGive(s_wifi_lock);
}

static void handle_sta_start(void)
{
    s_retry_num = 0;
    /* Only initiate connect when sf_wifi_connect() started the session.
     * In AP-only or APSTA provisioning mode this is suppressed. */
    if (s_initial_connect_phase) {
        esp_wifi_connect();
    }
}

static void handle_sta_disconnected(wifi_event_sta_disconnected_t *disc)
{
    bool auth_fail = (disc->reason == WIFI_REASON_AUTH_FAIL);

    if (s_initial_connect_phase) {
        if (auth_fail) {
            /* Definitive credential failure — 0 retries, unblock sf_wifi_connect(). */
            ESP_LOGW(TAG, "Auth fail (reason %d)", disc->reason);
            xEventGroupSetBits(s_wifi_event_group, WIFI_AUTH_FAIL_BIT);
        } else if (s_retry_num < SF_WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry %d/%d (reason %d)",
                     s_retry_num, SF_WIFI_MAXIMUM_RETRY, disc->reason);
        } else {
            ESP_LOGW(TAG, "Connect retries exhausted (reason %d)", disc->reason);
            /* Only signal the waiter; sf_wifi_connect() arms the perpetual
             * reconnect after it clears s_initial_connect_phase, so the initial
             * session and the reconnect session never overlap. */
            xEventGroupSetBits(s_wifi_event_group, WIFI_NO_AP_BIT);
        }
    } else {
        if (auth_fail) {
            s_auth_fail_count++;
            if (s_auth_fail_count >= SF_WIFI_PERPETUAL_AUTH_FAIL_MAX) {
                /* Repeated auth failures — credentials are likely wrong. */
                s_auth_fail_count = 0;
                ESP_LOGW(TAG, "Auth fail after %d attempts (reason %d), signalling",
                         SF_WIFI_PERPETUAL_AUTH_FAIL_MAX, disc->reason);
                if (esp_event_post(SF_WIFI_EVENT, SF_WIFI_EVENT_AUTH_FAIL,
                                   NULL, 0, pdMS_TO_TICKS(100)) != ESP_OK) {
                    ESP_LOGW(TAG, "post AUTH_FAIL failed");
                }
            } else {
                /* Transient auth glitch — back off and retry. */
                ESP_LOGW(TAG, "Auth fail on reconnect (reason %d), retry %d/%d",
                         disc->reason, s_auth_fail_count, SF_WIFI_PERPETUAL_AUTH_FAIL_MAX);
                if (esp_timer_start_once(s_reconnect_timer,
                                         (uint64_t)s_reconnect_delay_ms * 1000ULL) == ESP_OK) {
                    s_reconnect_delay_ms = (s_reconnect_delay_ms < SF_WIFI_RECONNECT_DELAY_MAX_MS)
                                         ? s_reconnect_delay_ms * 2
                                         : SF_WIFI_RECONNECT_DELAY_MAX_MS;
                }
            }
        } else {
            ESP_LOGI(TAG, "Disconnected, reconnect in %"PRIu32" ms (reason %d)",
                     s_reconnect_delay_ms, disc->reason);
            if (esp_event_post(SF_WIFI_EVENT, SF_WIFI_EVENT_DISCONNECTED,
                               NULL, 0, pdMS_TO_TICKS(100)) != ESP_OK) {
                ESP_LOGW(TAG, "post DISCONNECTED failed");
            }
            if (esp_timer_start_once(s_reconnect_timer,
                                     (uint64_t)s_reconnect_delay_ms * 1000ULL) == ESP_OK) {
                s_reconnect_delay_ms = (s_reconnect_delay_ms < SF_WIFI_RECONNECT_DELAY_MAX_MS)
                                     ? s_reconnect_delay_ms * 2
                                     : SF_WIFI_RECONNECT_DELAY_MAX_MS;
            }
        }
    }
}

static void handle_sta_got_ip(ip_event_got_ip_t *event)
{
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    s_reconnect_delay_ms = SF_WIFI_RECONNECT_DELAY_INIT_MS;
    s_auth_fail_count    = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    if (!s_initial_connect_phase) {
        if (esp_event_post(SF_WIFI_EVENT, SF_WIFI_EVENT_CONNECTED,
                           NULL, 0, pdMS_TO_TICKS(100)) != ESP_OK) {
            ESP_LOGW(TAG, "post CONNECTED failed");
        }
    }
}

/* Note: if sf_wifi_connect() is called while WiFi is already started,
 * STA_START does not fire and s_retry_num is not reset; the counter carries
 * over, which is acceptable since that path is not exercised in the current
 * provisioning design. */
static void event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                handle_sta_start();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                SF_CHECK_NULL_RETURN(ESP_LOGE, TAG, event_data, "STA_DISCONNECTED with NULL data");
                handle_sta_disconnected((wifi_event_sta_disconnected_t *)event_data);
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                SF_CHECK_NULL_RETURN(ESP_LOGE, TAG, event_data, "STA_GOT_IP with NULL data");
                handle_sta_got_ip((ip_event_got_ip_t *)event_data);
                break;
            default:
                break;
        }
    }
}

/* ------------------------------------------------------------------ */

sf_err_t sf_wifi_driver_init(void)
{
    if (s_driver_initialized) {
        return SF_OK;
    }

    esp_err_t status;

    s_wifi_event_group = xEventGroupCreate();
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, s_wifi_event_group, FAIL,
                       "Failed to create WiFi event group");

    s_wifi_lock = xSemaphoreCreateMutex();
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, s_wifi_lock, FAIL,
                       "Failed to create WiFi lock");

    /* Both calls return ESP_ERR_INVALID_STATE if already initialised by
     * another component — treat that as success, not an error. */
    status = esp_netif_init();
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG,
                       (status != ESP_OK && status != ESP_ERR_INVALID_STATE),
                       FAIL, "esp_netif_init failed: %d", status);

    status = esp_event_loop_create_default();
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG,
                       (status != ESP_OK && status != ESP_ERR_INVALID_STATE),
                       FAIL, "esp_event_loop_create_default failed: %d", status);

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, sta_netif, FAIL,
                       "Failed to create default WiFi STA netif");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    status = esp_wifi_init(&cfg);
    SF_CHECK_ERR_GOTO(ESP_LOGV, TAG, status, FAIL,
                      "esp_wifi_init status: %d", status);

    status = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                  &event_handler, NULL, NULL);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, status, FAIL,
                      "Register WIFI_EVENT handler: %d", status);

    status = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                  &event_handler, NULL, NULL);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, status, FAIL,
                      "Register IP_EVENT handler: %d", status);

    /* TODO(#65): replace with sf_timer once it supports task-context callbacks,
     * one-shot mode, and multiple independent instances. */
    esp_timer_create_args_t timer_args = {
        .callback = reconnect_timer_cb,
        .name     = "wifi_reconnect",
    };
    status = esp_timer_create(&timer_args, &s_reconnect_timer);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, status, FAIL,
                      "Create reconnect timer: %d", status);

    s_driver_initialized = true;
    ESP_LOGI(TAG, "Driver initialized.");

    return SF_OK;

FAIL:
    return SF_FAIL;
}

sf_wifi_conn_status_t sf_wifi_connect(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "Connecting to SSID: %s", ssid);

    s_initial_connect_phase = true;

    xEventGroupClearBits(s_wifi_event_group,
                         WIFI_CONNECTED_BIT | WIFI_AUTH_FAIL_BIT | WIFI_NO_AP_BIT);

    /* ssid/password are uint8_t[] arrays — cannot be set from a runtime
     * const char * in a struct initializer; strlcpy is required. */
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e        = WPA3_SAE_PWE_BOTH,
            .listen_interval    = 3,
        },
    };
    strlcpy((char *)wifi_config.sta.ssid,     ssid,     sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    /* Hold the lock only across the radio mutations — never across the wait
     * below, or ap_start/reconnect would be blocked for the whole boot. */
    xSemaphoreTake(s_wifi_lock, portMAX_DELAY);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    esp_err_t start_err = esp_wifi_start();
    if (start_err == ESP_ERR_WIFI_CONN) {
        /* Already started — WIFI_EVENT_STA_START won't fire, connect directly.
         * TODO: verify ESP_ERR_WIFI_CONN is the correct "already started" code
         * on IDF 5.4.2; this path is untested in the current design. */
        esp_wifi_connect();
    } else if (start_err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %d", start_err);
        s_initial_connect_phase = false;
        xSemaphoreGive(s_wifi_lock);
        return SF_WIFI_CONN_FAIL;
    }

    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

    xSemaphoreGive(s_wifi_lock);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                            WIFI_CONNECTED_BIT | WIFI_AUTH_FAIL_BIT | WIFI_NO_AP_BIT,
                                            pdFALSE, pdFALSE, portMAX_DELAY);

    /* Clear flag BEFORE returning so the perpetual handler activates cleanly. */
    s_initial_connect_phase = false;

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to SSID: %s", ssid);
        return SF_WIFI_CONN_OK;
    } else if (bits & WIFI_AUTH_FAIL_BIT) {
        ESP_LOGW(TAG, "Auth fail for SSID: %s", ssid);
        return SF_WIFI_CONN_AUTH_FAIL;
    }

    ESP_LOGW(TAG, "Failed to connect to SSID: %s", ssid);
    /* Session is over (s_initial_connect_phase cleared above) — now hand off to
     * perpetual self-heal. The timer's esp_wifi_connect() lands in the perpetual
     * branch. */
    esp_timer_start_once(s_reconnect_timer,
                         (uint64_t)s_reconnect_delay_ms * 1000ULL);
    return SF_WIFI_CONN_NO_AP;
}

sf_err_t sf_wifi_ap_start(const char *ssid)
{
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, s_ap_netif, FAIL,
                           "Failed to create AP netif");
    }

    wifi_config_t ap_config = {
        .ap = {
            .channel        = 1,
            .authmode       = WIFI_AUTH_OPEN,
            .max_connection = 4,
        },
    };
    strlcpy((char *)ap_config.ap.ssid, ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = (uint8_t)strlen(ssid);

    esp_err_t status;
    /* Serialize the get_mode→set_mode read-modify-write against a concurrent
     * reconnect on the esp_timer task. */
    xSemaphoreTake(s_wifi_lock, portMAX_DELAY);

    /* Use APSTA if STA is already running, AP-only otherwise. */
    wifi_mode_t mode = WIFI_MODE_AP;
    esp_wifi_get_mode(&mode);
    status = esp_wifi_set_mode(mode == WIFI_MODE_STA ? WIFI_MODE_APSTA : WIFI_MODE_AP);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, status, FAIL, "Set AP mode: %d", status);

    status = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, status, FAIL, "Set AP config: %d", status);

    /* Start WiFi; safe to call even if already started (returns error, no-op). */
    esp_wifi_start();

    xSemaphoreGive(s_wifi_lock);
    ESP_LOGI(TAG, "SoftAP started: SSID=%s", ssid);
    return SF_OK;

FAIL:
    xSemaphoreGive(s_wifi_lock);
    return SF_FAIL;
}

sf_err_t sf_wifi_ap_stop(void)
{
    xSemaphoreTake(s_wifi_lock, portMAX_DELAY);
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_wifi_get_mode(&mode);
    /* APSTA → STA; AP-only (no-creds path, STA never started) → NULL. */
    wifi_mode_t target = (mode == WIFI_MODE_APSTA) ? WIFI_MODE_STA : WIFI_MODE_NULL;
    esp_err_t status = esp_wifi_set_mode(target);
    xSemaphoreGive(s_wifi_lock);
    SF_CHECK_ERR_RETURN_FAIL(ESP_LOGE, TAG, status, "Set mode after AP stop: %d", status);
    ESP_LOGI(TAG, "SoftAP stopped.");
    return SF_OK;
}

sf_err_t sf_wifi_stop(void)
{
    if (s_reconnect_timer) {
        esp_timer_stop(s_reconnect_timer);
    }
    xSemaphoreTake(s_wifi_lock, portMAX_DELAY);
    esp_err_t status = esp_wifi_stop();
    xSemaphoreGive(s_wifi_lock);
    SF_CHECK_ERR_RETURN_FAIL(ESP_LOGI, TAG, status, "Stop wifi status: %d", status);
    ESP_LOGI(TAG, "WiFi stopped.");
    return SF_OK;
}

/* Deprecated wrapper — removed in commit 5 when sf_watering switches to
 * sf_wifi_prov_init(). Kconfig entries removed at the same time. */
sf_err_t sf_wifi_init(void)
{
    sf_err_t status = sf_wifi_driver_init();
    if (status != SF_OK) {
        return SF_FAIL;
    }
    sf_wifi_conn_status_t result = sf_wifi_connect(CONFIG_SIM_FLOW_WIFI_SSID,
                                                   CONFIG_SIM_FLOW_WIFI_PASSWORD);
    return (result == SF_WIFI_CONN_OK) ? SF_OK : SF_FAIL;
}
