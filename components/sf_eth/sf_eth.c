/**
 * @file sf_eth.c
 * @brief Ethernet initialization for QEMU (OpenCores MAC + DP83848 dummy PHY).
 */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_eth.h"

#include "sf_eth.h"

static const char *TAG = "SF_ETH";

#if CONFIG_ETH_USE_OPENETH

#include "esp_eth_mac_openeth.h"
#include "esp_eth_phy.h"

#define ETH_GOT_IP_BIT BIT0

static EventGroupHandle_t s_eth_event_group;

static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Link Up - HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Link Down");
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Stopped");
        break;
    default:
        break;
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    xEventGroupSetBits(s_eth_event_group, ETH_GOT_IP_BIT);
}

sf_err_t sf_eth_init(void)
{
    esp_err_t ret;

    s_eth_event_group = xEventGroupCreate();
    if (s_eth_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return SF_FAIL;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    if (eth_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create netif");
        return SF_FAIL;
    }

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_config);
    if (mac == NULL) {
        ESP_LOGE(TAG, "Failed to create MAC");
        return SF_FAIL;
    }

    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = -1;
    phy_config.reset_gpio_num = -1;
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);
    if (phy == NULL) {
        ESP_LOGE(TAG, "Failed to create PHY");
        return SF_FAIL;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ret = esp_eth_driver_install(&eth_config, &eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install driver: %d", ret);
        return SF_FAIL;
    }

    ret = esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach netif: %d", ret);
        return SF_FAIL;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    ret = esp_eth_start(eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Ethernet: %d", ret);
        return SF_FAIL;
    }

    ESP_LOGI(TAG, "Waiting for IP...");
    xEventGroupWaitBits(s_eth_event_group, ETH_GOT_IP_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    return SF_OK;
}

#else // CONFIG_ETH_USE_OPENETH

sf_err_t sf_eth_init(void)
{
    return SF_FAIL;
}

#endif // CONFIG_ETH_USE_OPENETH
