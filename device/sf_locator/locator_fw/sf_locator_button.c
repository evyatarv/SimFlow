#include "sf_locator.h"
#include "sf_locator_button.h"

#include "driver/gpio.h"
#include "esp_log.h"

#define DEBOUNCE_MS         50
#define DOUBLE_CLICK_MS     400
#define TRIPLE_CLICK_MS     400

static const char *TAG = "SF_LOCATOR_BTN";

static int          s_last_reading = 1;
static uint32_t     s_last_debounce_ms = 0;
static bool         s_press_handled = false;

static bool         s_pending_click = false;
static uint32_t     s_first_click_ms = 0;

static int          s_triple_count = 0;
static uint32_t     s_last_triple_ms = 0;

sf_err_t sf_locator_button_init(void)
{
    sf_err_t status = SF_FAIL;
    esp_err_t err;

    gpio_config_t io = {
        .intr_type      = GPIO_INTR_DISABLE,
        .mode           = GPIO_MODE_INPUT,
        .pin_bit_mask   = 1ULL << SF_LOCATOR_BUTTON_GPIO,
        .pull_up_en     = GPIO_PULLUP_ENABLE,
        .pull_down_en   = GPIO_PULLDOWN_DISABLE,
    };
    err = gpio_config(&io);
    SF_CHECK_ERR_GOTO(ESP_LOGE, TAG, err, end, "gpio_config with status: %d", err);

    s_last_reading = gpio_get_level(SF_LOCATOR_BUTTON_GPIO);

    status = SF_OK;
end:
    return status;
}

static bool poll_press_edge(uint32_t now_ms)
{
    int reading = gpio_get_level(SF_LOCATOR_BUTTON_GPIO);

    if (reading != s_last_reading) {
        s_last_debounce_ms = now_ms;
    }
    s_last_reading = reading;

    if (reading == 0 && (now_ms - s_last_debounce_ms) >= DEBOUNCE_MS) {
        if (!s_press_handled) {
            s_press_handled = true;
            return true;
        }
    } else if (reading == 1) {
        s_press_handled = false;
    }
    return false;
}

sf_button_event_t sf_locator_button_tick(uint32_t now_ms, bool triple_click_mode)
{
    bool just_pressed = poll_press_edge(now_ms);
    sf_button_event_t event = SF_BUTTON_EVENT_NONE;

    if (triple_click_mode) {
        if (just_pressed) {
            if ((now_ms - s_last_triple_ms) > TRIPLE_CLICK_MS) {
                s_triple_count = 0;
            }
            s_triple_count++;
            s_last_triple_ms = now_ms;
            ESP_LOGI(TAG, "Triple-click progress: %d/3", s_triple_count);
            if (s_triple_count >= 3) {
                s_triple_count = 0;
                event = SF_BUTTON_EVENT_TRIPLE_COMPLETE;
            } else {
                event = SF_BUTTON_EVENT_TRIPLE_PROGRESS;
            }
        }
        /* When in triple-click mode reset any pending single-click state. */
        s_pending_click = false;
        return event;
    }

    /* Normal mode: single / double click detection. */
    if (just_pressed) {
        if (!s_pending_click) {
            s_pending_click = true;
            s_first_click_ms = now_ms;
        } else {
            if ((now_ms - s_first_click_ms) <= DOUBLE_CLICK_MS) {
                event = SF_BUTTON_EVENT_DOUBLE;
                s_pending_click = false;
            } else {
                event = SF_BUTTON_EVENT_SINGLE;
                s_first_click_ms = now_ms;
            }
        }
    }

    /* Pending single-click whose window expired with no second press. */
    if (event == SF_BUTTON_EVENT_NONE && s_pending_click &&
        (now_ms - s_first_click_ms) > DOUBLE_CLICK_MS) {
        event = SF_BUTTON_EVENT_SINGLE;
        s_pending_click = false;
    }

    return event;
}
