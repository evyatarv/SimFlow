/**
 * @file main.c
 * @brief Simple Ethernet example for ESP32 QEMU emulator
 * 
 * This example demonstrates basic Ethernet initialization for QEMU.
 * QEMU provides emulated Ethernet interface that works without physical hardware.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"

// For QEMU, we use OpenCores MAC
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_com.h"

#include "sf_eth.h"

static const char *TAG = "SF_ETH";

// Event group for network connection
static EventGroupHandle_t s_eth_event_group;
#define ETH_CONNECTED_BIT BIT0
#define ETH_GOT_IP_BIT    BIT1

/**
 * @brief Ethernet event handler
 */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        xEventGroupSetBits(s_eth_event_group, ETH_CONNECTED_BIT);
        break;
        
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        xEventGroupClearBits(s_eth_event_group, ETH_CONNECTED_BIT | ETH_GOT_IP_BIT);
        break;
        
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
        
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        xEventGroupClearBits(s_eth_event_group, ETH_CONNECTED_BIT | ETH_GOT_IP_BIT);
        break;
        
    default:
        break;
    }
}

/**
 * @brief IP event handler
 */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    
    xEventGroupSetBits(s_eth_event_group, ETH_GOT_IP_BIT);
}

/**
 * @brief Initialize Ethernet for QEMU
 */
sf_err_t sf_eth_init(void)
{
    sf_err_t status = SF_FAIL;

    ESP_LOGI(TAG, "Initializing Ethernet for QEMU...");
    
     // Initialize NVS (required for network stack)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        status = nvs_flash_erase(); 
        SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != SF_OK, exit, "fail to erase nvs with status: %d", status);
        
        status = nvs_flash_init();
        SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != SF_OK, exit, "fail to init nvs with status: %d", status);
    }
    ESP_ERROR_CHECK(ret);

    // Create event group
    s_eth_event_group = xEventGroupCreate();
    
    // Initialize TCP/IP network interface
    status = esp_netif_init();
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != SF_OK, exit, "fail to init netif with status: %d", status);

    // Create default event loop
    status = esp_event_loop_create_default();
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != SF_OK, exit, "fail to init event create loop with status: %d", status);
    
    // Create default network interface for Ethernet
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, eth_netif, exit, "fail to create netif");

    // QEMU uses OpenCores MAC
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    
    // For QEMU emulator - use OpenCores Ethernet MAC
    esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_config);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, mac, exit, "fail to create mac");
    
    // QEMU doesn't need a real PHY, but we still need to provide one
    // Use a dummy PHY configuration
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = -1;  // No physical PHY in QEMU
    phy_config.reset_gpio_num = -1;
    
    // For QEMU, we can use DP83848 as dummy PHY
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
    SF_CHECK_NULL_GOTO(ESP_LOGE, TAG, eth_netif, exit, "fail to create phy");

    // Install Ethernet driver
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    status = esp_eth_driver_install(&eth_config, &eth_handle);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != SF_OK, exit, "fail to install eht deriver with status: %d", status);

    // Attach Ethernet driver to TCP/IP stack
    status = esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle));
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != SF_OK, exit, "fail to attach eth deriver with status: %d", status);

    // Register event handlers
    status = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != SF_OK, exit, "fail to register ESP_EVENT_ANY_ID with status: %d", status);

    status = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != SF_OK, exit, "fail to register IP_EVENT_ETH_GOT_IP with status: %d", status);

    // Start Ethernet driver
    status = esp_eth_start(eth_handle);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != SF_OK, exit, "fail to start ethernet with status: %d", status);

    // Wait for connection
    ESP_LOGI(TAG, "Waiting for Ethernet connection...");
    EventBits_t bits = xEventGroupWaitBits(s_eth_event_group,
                                           ETH_CONNECTED_BIT | ETH_GOT_IP_BIT,
                                           pdFALSE,
                                           pdTRUE,
                                           portMAX_DELAY);

    if (bits & ETH_CONNECTED_BIT) {
        ESP_LOGI(TAG, "✓ Ethernet connected!");
    }
    if (bits & ETH_GOT_IP_BIT) {
        ESP_LOGI(TAG, "✓ Got IP address!");
    }

exit:
    return(status);
}