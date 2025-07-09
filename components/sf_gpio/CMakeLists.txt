idf_component_register(
    SRCS "sf_gpio.c"
    INCLUDE_DIRS "." "../../include"
    REQUIRES esp_driver_gpio log)