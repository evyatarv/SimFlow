set(my_reqs "sf_files" "sf_cron_job" "sf_gpio" "sf_wifi" "sf_time" "nvs_flash" "log" "mqtt")

# --- DEBUG & LOGIC START ---
message(STATUS "-------------------------------------------------")
message(STATUS "[sf_watering] CHECKING EMULATION CONFIGURATION")
message(STATUS "[sf_watering] CMake Flag 'SF_EMULATION': ${SF_EMULATION}")
message(STATUS "[sf_watering] Env Var 'SF_EMULATION_BUILD': $ENV{SF_EMULATION_BUILD}")


# 1. Check Safe CMake Argument (Manual overrides)
if(SF_EMULATION)
    message(STATUS "[sf_watering] -> DETECTED: SF_EMULATION flag is set")
endif()

# 2. Check Environment Variable (VS Code / CI / Docker)
if(DEFINED ENV{SF_EMULATION_BUILD})
    message(STATUS "[sf_watering] -> DETECTED: SF_EMULATION_BUILD environment variable is set")
    list(APPEND my_reqs "sf_eth")
    message(STATUS "[sf_watering] STATUS: Emulation ENABLED -> Adding 'sf_eth' to requirements")    
else()
    message(STATUS "[sf_watering] STATUS: Emulation DISABLED")
endif()
message(STATUS "-------------------------------------------------")
# --- LOGIC END ---

idf_component_register(
    SRCS "watering_fw/sf_watering_hi.c" "watering_fw/sf_watering.c" "watering_fw/sf_watering_scheduler.c"
    INCLUDE_DIRS "." "../../include"
    REQUIRES ${my_reqs})

# 3. Apply the C Macro Definition
if(DEFINED ENV{SF_EMULATION_BUILD})
    idf_component_get_property(comp_lib ${COMPONENT_NAME} COMPONENT_LIB)
    target_compile_options(${comp_lib} PRIVATE "-DCONFIG_SIM_FLOW_EMULATION_BUILD=1")
endif()