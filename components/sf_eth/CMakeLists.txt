idf_component_register(
    SRCS "sf_eth.c"
    INCLUDE_DIRS "." "../../include"
    REQUIRES nvs_flash esp_eth esp_netif log)