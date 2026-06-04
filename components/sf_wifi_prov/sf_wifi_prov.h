#pragma once

#include "sf_err.h"

typedef enum {
    SF_WIFI_PROV_CLOSED = 0,   /* SoftAP off — normal operation             */
    SF_WIFI_PROV_PROVISIONING, /* SoftAP + HTTP server are up               */
} sf_wifi_prov_state_t;
/* The internal state machine runs three states (CLOSED / OPEN_INDEFINITE /
 * OPEN_TIMED — see DESIGN.md §6.1). Both OPEN states collapse to PROVISIONING
 * in this public enum: the caller only needs to know whether the SoftAP is up. */

/* All const char* fields are shallow-copied (pointer only). The strings they
 * point to must remain valid for the entire lifetime of the component (i.e.
 * they must be string literals or static storage). */
typedef struct {
    const char *ap_ssid;             /* SSID broadcast in provisioning mode    */

    /* UI content — served from the mounted FatFS. Absolute VFS paths. */
    const char *prov_html_path;      /* e.g. "/sf_fatfs/sf_watering/prov.html"    */
    const char *success_html_path;   /* e.g. ".../success.html"                   */
    const char *logo_path;           /* e.g. ".../logo.png"; NULL => no /logo.png */

    /* Invoked on SoftAP state transitions only (CLOSED ↔ PROVISIONING).
     * Used to toggle sf_pm light-sleep and notify sf_watering for LED.
     * May be NULL. Runs in event-loop context — keep it short. */
    void (*on_state_change)(sf_wifi_prov_state_t state, void *ctx);
    void  *state_ctx;
} sf_wifi_prov_config_t;

/* Boot entry point. Blocks through the bounded initial sf_wifi_connect()
 * attempt. Returns once the network subsystem is in motion. */
sf_err_t sf_wifi_prov_init(const sf_wifi_prov_config_t *config);

/* Manually open the provisioning server for a 5-min window (button press).
 * If already in OPEN_TIMED, this is a no-op — the window is not extended. */
sf_err_t sf_wifi_prov_start_server(void);

/* Erase stored credentials from NVS. Factory-reset hook only. */
sf_err_t sf_wifi_prov_clear(void);
