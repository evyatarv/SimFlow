#ifndef SF_LOCATOR_BUTTON_H
#define SF_LOCATOR_BUTTON_H

#include <stdint.h>
#include <stdbool.h>
#include "sf_err.h"

typedef enum {
    SF_BUTTON_EVENT_NONE = 0,
    SF_BUTTON_EVENT_SINGLE,
    SF_BUTTON_EVENT_DOUBLE,
    SF_BUTTON_EVENT_TRIPLE_PROGRESS,    /* triple-click pulse (one of 1..3) */
    SF_BUTTON_EVENT_TRIPLE_COMPLETE     /* third press within window */
} sf_button_event_t;

sf_err_t            sf_locator_button_init(void);

/* Poll the button, return any high-level event detected this tick.
 * The detection mode (normal vs. triple-click for wifi-lost) is selected
 * by `triple_click_mode`. */
sf_button_event_t   sf_locator_button_tick(uint32_t now_ms, bool triple_click_mode);

#endif /* SF_LOCATOR_BUTTON_H */
