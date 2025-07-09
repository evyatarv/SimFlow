
#include <stdint.h>
#include "sf_err.h"

#define CONFIG_GPIO_OUTPUT_0  18



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
sf_err_t sf_gpio_init();



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
void sf_gpio_set_level(uint32_t gpio_num, uint8_t level);

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
int sf_gpio_get_level(uint32_t gpio_num);