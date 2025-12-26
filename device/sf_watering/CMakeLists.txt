idf_component_register(
    SRCS "watering_fw/sf_watering_hi.c" "watering_fw/sf_watering.c" "watering_fw/sf_watering_scheduler.c" 
    INCLUDE_DIRS "." "../../include"
    REQUIRES sf_files sf_cron_job sf_gpio sf_wifi sf_time nvs_flash log mqtt)