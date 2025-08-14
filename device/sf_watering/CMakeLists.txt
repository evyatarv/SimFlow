idf_component_register(
    SRCS "sf_watering.c" "sf_watering_scheduler.c"
    INCLUDE_DIRS "." "../../include" "./include"
    REQUIRES sf_files sf_cron_job sf_gpio sf_wifi sf_time nvs_flash log)