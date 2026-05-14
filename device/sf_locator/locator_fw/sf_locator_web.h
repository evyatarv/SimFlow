#ifndef SF_LOCATOR_WEB_H
#define SF_LOCATOR_WEB_H

#include <stdint.h>
#include <stdbool.h>
#include "sf_err.h"

#define SF_LOCATOR_AP_SSID_DEFAULT  "simpleBot-AP"
#define SF_LOCATOR_AP_PASS_DEFAULT  "simplebot123"
#define SF_LOCATOR_AP_SSID_MAX_LEN  32
#define SF_LOCATOR_AP_PASS_MAX_LEN  64

sf_err_t        sf_locator_web_init(void);
sf_err_t        sf_locator_web_start_softap(void);

/* Process any pending soft-AP restart triggered by a /api/ap save. */
void            sf_locator_web_tick(uint32_t now_ms);

void            sf_locator_web_get_ap_ssid(char *out, size_t out_len);

#endif /* SF_LOCATOR_WEB_H */
