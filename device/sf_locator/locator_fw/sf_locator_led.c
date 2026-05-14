#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/rmt_tx.h"

#include "sf_locator.h"
#include "sf_locator_led.h"

#define LED_BLINK_INTERVAL_MS   500
#define LED_BRIGHTNESS          50      /* 0..255 software brightness scale */

/*
 * WS2812B bit-timing encoded as RMT symbols at a 10 MHz resolution
 * (one RMT tick == 0.1 us).
 *
 *  bit '0' : 0.40 us HIGH, 0.85 us LOW
 *  bit '1' : 0.80 us HIGH, 0.45 us LOW
 *  reset   : >50 us LOW between frames (latches data into the LED)
 *
 * Tolerance per datasheet is +/- 150 ns; the rounded tick counts below sit
 * inside that window.
 */
#define WS2812_RMT_RESOLUTION_HZ    10000000U
#define WS2812_T0H_TICKS            4       /* 0.40 us */
#define WS2812_T0L_TICKS            8       /* 0.80 us */
#define WS2812_T1H_TICKS            8       /* 0.80 us */
#define WS2812_T1L_TICKS            4       /* 0.40 us */
#define WS2812_RESET_TICKS          500     /* 50 us */
#define WS2812_NUM_BITS             24      /* 1 pixel * (G,R,B) * 8 */
#define WS2812_NUM_SYMBOLS          (WS2812_NUM_BITS + 1)
#define WS2812_RMT_MEM_BLOCK_SYM    64

static const char *TAG = "SF_LOCATOR_LED";

static rmt_channel_handle_t s_rmt_chan = NULL;
static rmt_encoder_handle_t s_copy_encoder = NULL;
static rmt_symbol_word_t    s_tx_buf[WS2812_NUM_SYMBOLS];

static sf_led_color_t   s_color = SF_LED_COLOR_RED;
static bool             s_led_on = false;
static bool             s_raw_active = false;
static uint8_t          s_raw_r = 255, s_raw_g = 0, s_raw_b = 0;
static bool             s_wifi_lost = false;
static bool             s_wifi_lost_red_phase = true;
static uint32_t         s_last_blink_ms = 0;

static const char *s_color_names[SF_LED_COLOR_COUNT] = {
    "red", "green", "blue", "off"
};

static uint8_t scale_brightness(uint8_t v)
{
    return (uint8_t)(((uint32_t)v * LED_BRIGHTNESS) / 255U);
}

static void encode_pixel(uint8_t r, uint8_t g, uint8_t b)
{
    /* WS2812 wire order is GRB, MSB first. */
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;

    for (int i = 0; i < WS2812_NUM_BITS; i++) {
        bool bit = (grb >> (WS2812_NUM_BITS - 1 - i)) & 1U;
        s_tx_buf[i].level0    = 1;
        s_tx_buf[i].level1    = 0;
        if (bit) {
            s_tx_buf[i].duration0 = WS2812_T1H_TICKS;
            s_tx_buf[i].duration1 = WS2812_T1L_TICKS;
        } else {
            s_tx_buf[i].duration0 = WS2812_T0H_TICKS;
            s_tx_buf[i].duration1 = WS2812_T0L_TICKS;
        }
    }

    /* Reset / latch pulse - long LOW, then end-of-frame (duration1 == 0). */
    s_tx_buf[WS2812_NUM_BITS].level0    = 0;
    s_tx_buf[WS2812_NUM_BITS].duration0 = WS2812_RESET_TICKS;
    s_tx_buf[WS2812_NUM_BITS].level1    = 0;
    s_tx_buf[WS2812_NUM_BITS].duration1 = 0;
}

static void write_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    if (s_rmt_chan == NULL || s_copy_encoder == NULL) {
        return;
    }
    encode_pixel(scale_brightness(r),
                 scale_brightness(g),
                 scale_brightness(b));

    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    rmt_transmit(s_rmt_chan, s_copy_encoder,
                 s_tx_buf, sizeof(s_tx_buf), &tx_cfg);
    rmt_tx_wait_all_done(s_rmt_chan, 100);
}

static void write_named(sf_led_color_t color, bool on)
{
    if (!on || color == SF_LED_COLOR_OFF) {
        write_rgb(0, 0, 0);
    } else if (color == SF_LED_COLOR_RED) {
        write_rgb(255, 0, 0);
    } else if (color == SF_LED_COLOR_GREEN) {
        write_rgb(0, 255, 0);
    } else {
        write_rgb(0, 0, 255);
    }
}

sf_err_t sf_locator_led_init(void)
{
    sf_err_t status = SF_FAIL;
    esp_err_t err;

    rmt_tx_channel_config_t chan_cfg = {
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .gpio_num          = SF_LOCATOR_LED_GPIO,
        .mem_block_symbols = WS2812_RMT_MEM_BLOCK_SYM,
        .resolution_hz     = WS2812_RMT_RESOLUTION_HZ,
        .trans_queue_depth = 1,
    };
    err = rmt_new_tx_channel(&chan_cfg, &s_rmt_chan);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end,
                      "rmt_new_tx_channel with status: %d", err);

    rmt_copy_encoder_config_t copy_cfg = { 0 };
    err = rmt_new_copy_encoder(&copy_cfg, &s_copy_encoder);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end,
                      "rmt_new_copy_encoder with status: %d", err);

    err = rmt_enable(s_rmt_chan);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end,
                      "rmt_enable with status: %d", err);

    write_rgb(0, 0, 0);
    s_color = SF_LED_COLOR_RED;
    s_led_on = true;
    write_named(s_color, s_led_on);

    status = SF_OK;
end:
    return status;
}

void sf_locator_led_tick(uint32_t now_ms)
{
    if (s_rmt_chan == NULL) {
        return;
    }
    if ((now_ms - s_last_blink_ms) < LED_BLINK_INTERVAL_MS) {
        return;
    }
    s_last_blink_ms = now_ms;

    if (s_wifi_lost) {
        s_wifi_lost_red_phase = !s_wifi_lost_red_phase;
        write_rgb(s_wifi_lost_red_phase ? 255 : 0, 0, s_wifi_lost_red_phase ? 0 : 255);
        return;
    }

    if (s_raw_active) {
        s_led_on = !s_led_on;
        if (s_led_on) {
            write_rgb(s_raw_r, s_raw_g, s_raw_b);
        } else {
            write_rgb(0, 0, 0);
        }
        return;
    }

    if (s_color != SF_LED_COLOR_OFF) {
        s_led_on = !s_led_on;
        write_named(s_color, s_led_on);
    }
}

void sf_locator_led_set_named(sf_led_color_t color)
{
    s_color = color;
    s_raw_active = false;
    s_wifi_lost = false;
    s_led_on = (color != SF_LED_COLOR_OFF);
    write_named(s_color, s_led_on);
}

void sf_locator_led_set_raw(uint8_t r, uint8_t g, uint8_t b)
{
    s_raw_r = r;
    s_raw_g = g;
    s_raw_b = b;
    s_raw_active = true;
    s_led_on = true;
    s_wifi_lost = false;
    write_rgb(r, g, b);
}

sf_led_color_t sf_locator_led_cycle(void)
{
    s_raw_active = false;
    s_color = (sf_led_color_t)((s_color + 1) % SF_LED_COLOR_COUNT);
    if (s_color == SF_LED_COLOR_OFF) {
        s_led_on = false;
        write_named(SF_LED_COLOR_OFF, false);
    } else {
        s_led_on = true;
        write_named(s_color, s_led_on);
    }
    return s_color;
}

void sf_locator_led_set_wifi_lost(bool enabled)
{
    s_wifi_lost = enabled;
    if (!enabled) {
        write_named(s_color, s_led_on);
    }
}

bool sf_locator_led_is_wifi_lost(void)
{
    return s_wifi_lost;
}

sf_led_color_t sf_locator_led_get_color(void)
{
    return s_color;
}

bool sf_locator_led_get_raw_active(void)
{
    return s_raw_active;
}

void sf_locator_led_get_raw(uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (r) *r = s_raw_r;
    if (g) *g = s_raw_g;
    if (b) *b = s_raw_b;
}

const char *sf_locator_led_color_name(sf_led_color_t color)
{
    if ((unsigned)color >= SF_LED_COLOR_COUNT) {
        return "?";
    }
    return s_color_names[color];
}
