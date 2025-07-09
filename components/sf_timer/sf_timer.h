#include "sf_err.h"
#include "driver/gptimer_types.h"

#define SF_TIMER_RESOLUTION_MICRO_S     1000000 // 1MHz, 1 tick=1us
#define SF_TIMER_RESOLUTION_MILI_S      1000 // 1KHz, 1 tick=1ms
#define SF_TIMER_RESOLUTION_SEC         1 // 1Hz, 1 tick=1s

/**
 * @brief GPIO common configuration
 *
 *        Configure GPIO's Mode,pull-up,PullDown,IntrType
 *
 * @param  pGPIOConfig Pointer to GPIO configure struct
 *
 * @note This function always overwrite all the current IO configurations
 *
 * @return
 *     - ESP_OK success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *
 */
sf_err_t sf_timer_init(gptimer_alarm_cb_t cb, uint32_t timer_resolution);


/**
 * @brief GPIO common configuration
 *
 *        Configure GPIO's Mode,pull-up,PullDown,IntrType
 *
 * @param  pGPIOConfig Pointer to GPIO configure struct
 *
 * @note This function always overwrite all the current IO configurations
 *
 * @return
 *     - ESP_OK success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *
 */
void sf_timer_start(uint64_t count);

/**
 * @brief GPIO common configuration
 *
 *        Configure GPIO's Mode,pull-up,PullDown,IntrType
 *
 * @param  pGPIOConfig Pointer to GPIO configure struct
 *
 * @note This function always overwrite all the current IO configurations
 *
 * @return
 *     - ESP_OK success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *
 */
void sf_timer_stop(); 

/**
 * @brief GPIO common configuration
 *
 *        Configure GPIO's Mode,pull-up,PullDown,IntrType
 *
 * @param  pGPIOConfig Pointer to GPIO configure struct
 *
 * @note This function always overwrite all the current IO configurations
 *
 * @return
 *     - ESP_OK success
 *     - ESP_ERR_INVALID_ARG Parameter error
 *
 */
void sf_timer_clear();