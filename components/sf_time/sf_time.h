#pragma once

#include <time.h>
#include "esp_event.h"
#include "sf_err.h"

/* Posted to the default event loop on the first invalid→valid transition.
 * Subscribe after sf_wifi_prov_init() (which creates the loop) returns. */
ESP_EVENT_DECLARE_BASE(SF_TIME_EVENT);

typedef enum {
    SF_TIME_EVENT_VALID = 0,  /* time became valid (SNTP success or manual set) */
} sf_time_event_id_t;

/* One-time init: RTC guard + SNTP setup. Call before any other sf_time API.
 * Does not start SNTP — call sf_time_sntp_restart() to trigger the first sync. */
sf_err_t sf_time_init(void);

/* Set timezone. Pass NULL to use the Kconfig default. */
sf_err_t sf_time_set_timezone(const char *timezone);

/* Returns true if time has been validated this power cycle (SNTP or manual).
 * Persists across esp_restart/sleep; cleared only on power loss (RTC domain). */
bool sf_time_is_valid(void);

/* Set time manually (e.g. from provisioning page). Marks valid, posts SF_TIME_EVENT_VALID. */
sf_err_t sf_time_set_manual(time_t epoch);

/* Re-trigger SNTP sync. No-op if already valid or offline (SNTP will retry internally).
 * Safe to call multiple times — starts SNTP on first call, forces resync on subsequent. */
sf_err_t sf_time_sntp_restart(void);

/* Print current time to log. */
void sf_time_print_current_time(void);

/* Return current time as ctime() string. Caller must not free, modify, or
 * store the pointer — ctime() returns a shared static buffer overwritten on
 * the next call from any context. */
char *sf_time_get_current_time(void);
