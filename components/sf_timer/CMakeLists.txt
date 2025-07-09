idf_component_register(
    SRCS "sf_timer.c"
    INCLUDE_DIRS "." "../../include"
    REQUIRES esp_driver_gptimer log)