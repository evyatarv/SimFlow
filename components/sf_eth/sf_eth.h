/**
 * @file sf_eth.h
 * @brief Ethernet initialization for ESP32 QEMU emulator.
 */

#ifndef SF_ETH_H
#define SF_ETH_H

#include "sf_err.h"

/**
 * @brief Initialize Ethernet for QEMU.
 * 
 * This function configures and starts the Ethernet driver for the QEMU
 * emulated environment. It handles MAC, PHY, and TCP/IP stack setup.
 * 
 * @return SF_OK on success, SF_FAIL on any error.
 */
sf_err_t sf_eth_init(void);

#endif // SF_ETH_H