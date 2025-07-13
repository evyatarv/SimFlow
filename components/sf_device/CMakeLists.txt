idf_component_register(
    SRCS "sf_device.c"
    INCLUDE_DIRS "." "../../include"
    REQUIRES sf_gpio sf_timer sf_wifi sf_time nvs_flash)