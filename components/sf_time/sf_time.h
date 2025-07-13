

#include "sf_err.h"




/**
 * @brief Set the system date and time using SNTP.
 *
 * This function synchronizes the device's system time with an SNTP server.
 *
 * @return sf_err_t SF_OK on success, SF_FAIL on failure.
 */
sf_err_t sf_time_set_sntp_date();



/**
 * @brief Print the current system date and time.
 *
 * This function outputs the current system time to the console or log.
 */
void sf_time_print_current_time();


/**
 * @brief Initialize the SimFlow time module.
 *
 * This function prepares the time module for use, including setting up SNTP.
 *
 * @return sf_err_t SF_OK on success, SF_FAIL on failure.
 */
sf_err_t sf_time_init();