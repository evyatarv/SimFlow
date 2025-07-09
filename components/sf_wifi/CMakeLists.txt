idf_component_register(
    SRCS "sf_wifi.c"
    INCLUDE_DIRS "." "../../include"
    REQUIRES esp_wifi log)