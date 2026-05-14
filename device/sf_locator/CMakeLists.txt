set(my_reqs
    "sf_files"
    "sf_gpio"
    "sf_time"
    "nvs_flash"
    "log"
    "driver"
    "esp_wifi"
    "esp_netif"
    "esp_event"
    "esp_http_server"
    "esp_http_client"
    "esp-tls"
    "mbedtls"
    "json")

idf_component_register(
    SRCS "locator_fw/sf_locator.c"
         "locator_fw/sf_locator_led.c"
         "locator_fw/sf_locator_mpu.c"
         "locator_fw/sf_locator_button.c"
         "locator_fw/sf_locator_telegram.c"
         "locator_fw/sf_locator_web.c"
    INCLUDE_DIRS "." "locator_fw" "../../include"
    EMBED_TXTFILES "locator_app/webui/index.html"
    REQUIRES ${my_reqs})
