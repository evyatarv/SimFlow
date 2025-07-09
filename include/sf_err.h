/**
 * @file sf_err.h
 * @brief Error handling definitions and macros for the SimFlow project.
 *
 * This file provides error codes, types, and utility macros to handle errors
 * consistently across the SimFlow project. It includes definitions for common
 * error codes, as well as macros for error checking and logging.
 */

typedef int sf_err_t; /*!< Type definition for error codes used in SimFlow. */

/**
 * @brief Macro to check an error condition and jump to a label if the condition is met.
 *
 * @param LOG_TYPE Logging function to use (e.g., ESP_LOGE, ESP_LOGW).
 * @param TAG Tag for the log message.
 * @param status The status to check against ESP_OK.
 * @param goto_label The label to jump to if the status is not ESP_OK.
 * @param msg The log message format string.
 * @param ... Additional arguments for the log message.
 */
#define SF_CHECK_ERR_GOTO(LOG_TYPE, TAG, status, goto_label, msg, ...) \
    LOG_TYPE(TAG, msg, ##__VA_ARGS__); \
    if ((status) != ESP_OK) { \
        status = SF_FAIL; \
        goto goto_label; \
    }

/**
 * @brief Macro to check an error condition and return the status if the condition is met.
 *
 * @param LOG_TYPE Logging function to use (e.g., ESP_LOGE, ESP_LOGW).
 * @param TAG Tag for the log message.
 * @param status The status to check against ESP_OK.
 * @param msg The log message format string.
 * @param ... Additional arguments for the log message.
 */
#define SF_CHECK_ERR_RETURN(LOG_TYPE, TAG, status, msg, ...) \
    LOG_TYPE(TAG, msg, ##__VA_ARGS__); \
    if ((status) != ESP_OK) { \
        return SF_FAIL; \
    }

/* Definitions for error constants. */

/** @brief sf_err_t value indicating success (no error). */
#define SF_OK           0

/** @brief Generic sf_err_t code indicating failure. */
#define SF_FAIL        -1

/** @brief Starting number of GPIO error codes. */
#define SF_ERR_GPIO_BASE                   0x3000

/** @brief Starting number of timer error codes. */
#define SF_ERR_TIMER_BASE                  0x4000

/** @brief Error code indicating invalid parameter for timer operations. */
#define SF_ERR_TIMER_INVAL_PARAM           0x4001

