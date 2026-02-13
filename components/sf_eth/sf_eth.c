#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_eth_mac_openeth.h"
#include "sf_eth.h"


static const char* TAG = "SF_ETHERNET";

static esp_eth_handle_t g_eth_handle = NULL;


sf_err_t sf_eth_init()
{
    // Create the network interface for Ethernet
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);
    sf_err_t status = SF_FAIL; 


    // Configure the MAC layer for OpenEth
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_config);

    ESP_LOGI(TAG, "sf initializing Eth ...");

    // Configure the PHY 
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    esp_eth_phy_t *phy = esp_eth_phy_new_dp83848(&phy_config);

    // Install the driver
    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    
    //status = esp_eth_driver_install(&eth_config, &g_eth_handle);
    SF_CHECK_EXPR_RETURN_VAL(ESP_LOGE, TAG, status != ESP_OK, SF_FAIL, "Ethernet driver install exit with status %d", status);

    // Attach the netif
    status = esp_netif_attach(eth_netif, esp_eth_new_netif_glue(g_eth_handle));
    SF_CHECK_EXPR_RETURN_VAL(ESP_LOGE, TAG, status != ESP_OK, SF_FAIL, "Ethernet netif attach exit with status %d", status);

    // Start the driver
    status = esp_eth_start(g_eth_handle);
    SF_CHECK_EXPR_RETURN_VAL(ESP_LOGE, TAG, status != ESP_OK, SF_FAIL, "Ethernet start exit with status %d", status);

    return SF_OK;
}


sf_err_t sf_eth_stop(void)
{    
    SF_CHECK_EXPR_RETURN_VAL(ESP_LOGE, TAG, g_eth_handle == NULL, SF_FAIL, "Ethernet handles cannot be NULL");
        
    esp_eth_mac_t *mac = NULL;
    esp_eth_phy_t *phy = NULL;
    sf_err_t status = SF_FAIL; 

    status = esp_eth_stop(g_eth_handle);
    SF_CHECK_EXP_NO_RETURN_STATUS(ESP_LOGI, TAG, status != ESP_OK, "Ethernet stop exit with status %d", status);
    
    esp_eth_get_mac_instance(g_eth_handle, &mac);
    esp_eth_get_phy_instance(g_eth_handle, &phy);

    status = esp_eth_driver_uninstall(g_eth_handle);
    SF_CHECK_EXPR_RETURN_VAL(ESP_LOGE, TAG, status != ESP_OK, SF_FAIL, "Ethernet driver uninstall exit with status %d", status);

    if (mac != NULL) {
        mac->del(mac);
    }
    if (phy != NULL) {
        phy->del(phy);
    }
    
    free(g_eth_handle);
    g_eth_handle = NULL;

    return ESP_OK;
}
