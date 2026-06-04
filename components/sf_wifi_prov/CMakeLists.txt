idf_component_register(
    SRCS "sf_wifi_prov.c"
    INCLUDE_DIRS "." "../../include"
    REQUIRES esp_wifi esp_netif esp_event esp_http_server nvs_flash esp_timer log sf_wifi sf_time)
