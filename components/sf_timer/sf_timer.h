
// Error codes and definitions for SimFlow timer
#include "sf_err.h"
// ESP-IDF GPTimer types for timer configuration and callbacks
#include "driver/gptimer_types.h"

// Timer resolution macros
#define SF_TIMER_RESOLUTION_MICRO_S     1000000 // 1MHz, 1 tick = 1us
#define SF_TIMER_RESOLUTION_MILI_S      1000    // 1KHz, 1 tick = 1ms
#define SF_TIMER_RESOLUTION_SEC         1       // 1Hz, 1 tick = 1s


/**
 * @brief Initialize the SimFlow timer
 *
 * @param cb            Callback function for timer alarm events (ISR)
 * @param cb_user_data  User data pointer passed to the callback
 * @param timer_resolution Timer tick resolution (use macros above)
 *
 * @return sf_err_t     SF_OK on success, SF_FAIL or ESP_ERR_INVALID_ARG on error
 */
sf_err_t sf_timer_init(gptimer_alarm_cb_t cb, uint32_t cb_user_data_size, uint32_t timer_resolution);



/**
 * @brief Start the SimFlow timer
 *
 * @param count Number of timer ticks before alarm triggers
 *
 * This function starts the timer with the specified count value.
 */
void sf_timer_start(uint64_t count);


/**
 * @brief Stop the SimFlow timer
 *
 * This function stops the timer and disables further alarm events.
 */
void sf_timer_stop();


/**
 * @brief Clear the SimFlow timer
 *
 * This function resets the timer counter to zero.
 */
void sf_timer_clear();


/**
 * @brief Retrieve user data from the SimFlow timer
 *
 * @param user_data   Pointer to buffer where user data will be copied
 * @param ticks_wait  Number of ticks to wait for data (timeout)
 *
 * This function allows tasks to obtain user data passed from the timer ISR or other timer events.
 */
void sf_timer_get_user_data(void *user_data, uint32_t ticks_wait);




