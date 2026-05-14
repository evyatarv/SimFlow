#ifndef SF_LOCATOR_LED_H
#define SF_LOCATOR_LED_H

#include <stdint.h>
#include <stdbool.h>
#include "sf_err.h"

typedef enum {
    SF_LED_COLOR_RED = 0,
    SF_LED_COLOR_GREEN,
    SF_LED_COLOR_BLUE,
    SF_LED_COLOR_OFF,
    SF_LED_COLOR_COUNT
} sf_led_color_t;

sf_err_t        sf_locator_led_init(void);

/* Blink/effect tick - call regularly from the main loop with millis(). */
void            sf_locator_led_tick(uint32_t now_ms);

/* Named color (cycle palette) - returns to normal blink mode. */
void            sf_locator_led_set_named(sf_led_color_t color);

/* Raw RGB - enters raw blink mode. */
void            sf_locator_led_set_raw(uint8_t r, uint8_t g, uint8_t b);

/* Cycle to next named color (RED -> GREEN -> BLUE -> OFF -> ...). Returns
 * the new color so callers can format a message. */
sf_led_color_t  sf_locator_led_cycle(void);

/* Toggle the wifi-lost alert pattern (red/blue alternating). */
void            sf_locator_led_set_wifi_lost(bool enabled);
bool            sf_locator_led_is_wifi_lost(void);

/* Accessors used by the web /api/status handler. */
sf_led_color_t  sf_locator_led_get_color(void);
bool            sf_locator_led_get_raw_active(void);
void            sf_locator_led_get_raw(uint8_t *r, uint8_t *g, uint8_t *b);

const char *    sf_locator_led_color_name(sf_led_color_t color);

#endif /* SF_LOCATOR_LED_H */
