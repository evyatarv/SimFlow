idf_component_register(
    SRCS "sf_files.c"
    INCLUDE_DIRS "." "../../include"
    REQUIRES fatfs vfs log)