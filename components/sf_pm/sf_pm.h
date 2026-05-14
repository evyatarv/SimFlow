#pragma once

#include "sf_err.h"
#include <stdbool.h>


/**
 * @brief Configure dynamic frequency scaling and automatic light sleep.
 *
 * Applies an esp_pm configuration with the CPU max frequency set to
 * CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ and the min frequency dropped to
 * CONFIG_XTAL_FREQ when idle. When @p enable is true, the chip will
 * automatically enter light sleep whenever no task is ready to run and
 * no peripheral is locking the power state.
 *
 * Requires CONFIG_PM_ENABLE to be set in sdkconfig. If power management
 * is not enabled in the build, the call is a no-op that logs a warning
 * and returns SF_OK.
 *
 * @param[in] enable  true  - enable automatic light sleep.
 *                    false - keep DFS active but disable automatic light sleep.
 *
 * @return SF_OK on success, or an SF error code propagated from
 *         esp_pm_configure() on failure.
 */
sf_err_t sf_pm_set_light_sleep_power_mode(bool enable);