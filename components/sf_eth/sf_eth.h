/**
 * @file sf_eth.h
 * @brief Ethernet initialization for ESP32 with internal MAC
 */

#ifndef SF_ETH_H
#define SF_ETH_H
#include <stdbool.h>
#include "sf_err.h"

/**
 * @brief Initialize Ethernet with internal MAC and external PHY
 * 
 * This function initializes the Ethernet interface using ESP32's built-in
 * Ethernet MAC controller and an external PHY chip (LAN8720, RTL8201, etc.)
 * 
 * @return
 *     - ESP_OK: Success
 *     - ESP_FAIL: Initialization failed
 */
sf_err_t sf_eth_init(void);

/**
 * @brief Check if Ethernet is connected and has IP
 * 
 * @return true if connected with valid IP, false otherwise
 */
//bool sf_eth_is_connected(void);

/**
 * @brief Get Ethernet IP address as string
 * 
 * @param ip_str Buffer to store IP string (minimum 16 bytes)
 * @return ESP_OK on success, ESP_FAIL if not connected
 */
//sf_err_t sf_eth_get_ip(char *ip_str);


#endif // SF_ETH_H