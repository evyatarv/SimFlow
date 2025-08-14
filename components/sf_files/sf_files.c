#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"

#include "sf_files.h"

static const char *TAG = "SF_FILES";

static wl_handle_t wl_handle = WL_INVALID_HANDLE;

#define SF_FAT_PARTITION_LABLE  "storage"

sf_err_t sf_file_init_fs(const char* base_path)
{
     const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = false,
            .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
            .use_one_fat = false,
     };
    
    esp_err_t status = esp_vfs_fat_spiflash_mount_rw_wl(base_path, SF_FAT_PARTITION_LABLE, &mount_config, &wl_handle);
    SF_CHECK_ERR_RETURN_STATUS(ESP_LOGI, TAG, status, "FS FAT mounting base path %s to read/write end with status %d", base_path, status);
}



sf_err_t sf_file_deinit_fs(const char* base_path)
{
    esp_err_t status  = esp_vfs_fat_spiflash_unmount_rw_wl(base_path, wl_handle);
    wl_handle = WL_INVALID_HANDLE;
    SF_CHECK_ERR_RETURN_STATUS(ESP_LOGI, TAG, status, "FS FAT unmounting base path %s end with status: %d", base_path, status);
}