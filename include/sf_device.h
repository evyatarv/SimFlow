
#include "sf_err.h"

/**
 * @brief GPIO common configuration
 *
 *        Configure GPIO's Mode,pull-up,PullDown,IntrType
 *
 */
typedef union sf_device_cfg_t
{
    struct {
        unsigned int reserved: 32;
    };
    unsigned int dev_cfg;

} sf_device_cfg_t;

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
extern sf_err_t init_device(sf_device_cfg_t dev_cfg);

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
extern sf_err_t device_start ();