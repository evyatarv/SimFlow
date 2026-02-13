
set(my_reqs "sf_files" "sf_cron_job" "sf_gpio" "sf_wifi" "sf_time" "nvs_flash" "log" "mqtt")
message(STATUS "---> my_reqs (raw) = ${my_reqs}")


if(CONFIG_SIM_FLOW_EMULATION_BUILD)
    list(APPEND my_reqs "sf_eth")
endif()

message(STATUS "---> my_reqs (raw) = ${my_reqs}")

idf_component_register(
    SRCS "watering_fw/sf_watering_hi.c" "watering_fw/sf_watering.c" "watering_fw/sf_watering_scheduler.c" 
    INCLUDE_DIRS "." "../../include"
    REQUIRES ${my_reqs})

if(CONFIG_SIM_FLOW_EMULATION_BUILD)
    idf_component_get_property(comp_lib ${COMPONENT_NAME} COMPONENT_LIB)
    target_compile_options(${comp_lib} PRIVATE "-DCONFIG_SIM_FLOW_EMULATION_BUILD=1")
endif()