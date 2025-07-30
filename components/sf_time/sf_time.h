

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



/**
 * @brief Set the system timezone.
 *
 * This function sets the system timezone for time calculations and formatting.
 *
 * @param timezone   A string representing the timezone (e.g., "UTC", "Europe/Berlin").
 * @return sf_err_t  SF_OK on success, SF_FAIL on failure.
 */
sf_err_t sf_time_set_timezone(const char* timezone);



/**
 * @brief Get the current system time as a formatted string.
 *
 * This function retrieves the current system time and returns it as a formatted
 * string representation. The returned string contains the date and time
 * according to the system's current timezone settings.
 *
 * @return char* A pointer to a string containing the formatted date and time.
 *         Returns NULL if the system time is not yet set or on error.
 *         Note: The caller should not modify or free the returned string.
 */
char*  sf_time_get_current_time();
