idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "../include"
    REQUIRES sf_watering log)




set(image ../sf_fatfs)

fatfs_create_spiflash_image(storage ${image} FLASH_IN_PROJECT PRESERVE_TIME)