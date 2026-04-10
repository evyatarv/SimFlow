#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include <errno.h>

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

sf_err_t sf_file_read(const char* file, uint8_t* buff, uint32_t* size, uint32_t offset)
{
    sf_err_t status = SF_FAIL;
    FILE *f = NULL;
    struct stat file_info = {0};
    int fd  = -1;

    f = fopen(file, "rb");
    SF_CHECK_NULL_RETURN_ERR(ESP_LOGE, TAG, f, "Failed to open file: %s with error: %d", file, errno);
    
    fd = fileno(f);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, fd == -1, end, "fail to get file descriptor with error: %d", errno);

    status = fstat(fd, &file_info);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != ESP_OK, end, "fail to get file info with error: %d", errno);

    uint32_t overflow_check = 0;
    if (offset > (uint32_t)file_info.st_size
        || __builtin_add_overflow(*size, offset, &overflow_check))
    {
        ESP_LOGE(TAG, "READ: invalid size or offset. size: %lu | offset: %lu", *size, offset);
        goto end;
    }

    status = fseek(f, offset, SEEK_SET);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != ESP_OK, end, "fail to seek file with error: %d", errno);

    *size = (uint32_t)fread(buff, 1, *size, f);
    status = SF_OK;



end:
    fclose(f);
    return status;
}

sf_err_t sf_file_write(const char* file, uint8_t* buff, uint32_t* size, uint32_t offset)
{
    sf_err_t status = SF_FAIL;
    FILE *f = NULL;
    struct stat file_info = {0};
    int fd = -1;

    f = fopen(file, "r+b");
    if (f == NULL)
    {
        ESP_LOGI(TAG, "File %s not found, creating new file", file);
        f = fopen(file, "wb");
    }
    SF_CHECK_NULL_RETURN_ERR(ESP_LOGE, TAG, f, "Failed to open file: %s with error: %d", file, errno);

    fd = fileno(f);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, fd == -1, end, "fail to get file descriptor with error: %d", errno);

    status = fstat(fd, &file_info);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != ESP_OK, end, "fail to get file info with error: %d", errno);

    uint32_t overflow_check = 0;
    if (__builtin_add_overflow(*size, offset, &overflow_check))
    {
        ESP_LOGE(TAG, "WRITE: invalid size or offset. size: %lu | offset: %lu", *size, offset);
        goto end;
    }

    status = fseek(f, offset, SEEK_SET);
    SF_CHECK_EXPR_GOTO(ESP_LOGE, TAG, status != ESP_OK, end, "fail to seek file with error: %d", errno);

    *size = (uint32_t)fwrite(buff, 1, *size, f);
    status = SF_OK;

end:
    fclose(f);
    return status;
}