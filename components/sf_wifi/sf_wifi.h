#pragma once

#include "esp_event.h"
#include "sf_err.h"

typedef enum {
    SF_WIFI_CONN_OK = 0,
    SF_WIFI_CONN_AUTH_FAIL,  /* WIFI_REASON_AUTH_FAIL (202) only              */
    SF_WIFI_CONN_NO_AP,      /* SSID not found, AP unreachable, or transient  */
    SF_WIFI_CONN_FAIL,       /* unexpected failure                            */
} sf_wifi_conn_status_t;

/* Events posted to the default event loop after sf_wifi_connect() returns.
 * Suppressed during the blocking initial connect — subscribe after
 * sf_wifi_prov_init() (which creates the loop) returns. */
ESP_EVENT_DECLARE_BASE(SF_WIFI_EVENT);

typedef enum {
    SF_WIFI_EVENT_CONNECTED = 0, /* STA got IP — perpetual phase only            */
    SF_WIFI_EVENT_DISCONNECTED,  /* STA disconnected — perpetual phase only       */
    SF_WIFI_EVENT_AUTH_FAIL,     /* auth fail in perpetual reconnect — stop retry */
} sf_wifi_event_id_t;

/* One-time stack init: netif, event loop, STA netif, esp_wifi_init, event
 * handlers, reconnect timer. Guarded — safe to call again (no-op). */
sf_err_t sf_wifi_driver_init(void);

/* Blocking STA connect. WIFI_REASON_AUTH_FAIL (202): 0 retries, returns
 * immediately. All other failures: up to 5 retries before returning.
 * After returning, the perpetual reconnect handler takes over for NO_AP. */
sf_wifi_conn_status_t sf_wifi_connect(const char *ssid, const char *password);

/* Switch to APSTA, create AP netif (once, guarded), configure and start AP.
 * Only sf_wifi_prov may call this — never touch esp_wifi_* directly. */
sf_err_t sf_wifi_ap_start(const char *ssid);

/* Stop the AP, switch back to STA-only mode. */
sf_err_t sf_wifi_ap_stop(void);

/* Stop the WiFi driver. Cancels any pending reconnect timer.
 * After this call, sf_wifi_connect() or sf_wifi_ap_start() may be called
 * again to restart. */
sf_err_t sf_wifi_stop(void);
