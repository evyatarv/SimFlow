
#ifndef SF_FILES_H
#define SF_FILES_H

#include "sf_err.h"

/**
 * @file sf_files.h
 * @brief SimFlow file system management component.
 *
 * This component provides functions for initializing and deinitializing the file system,
 * using FAT partition.
 */

/**
 * @brief Initializes the file system on the specified base path.
 *
 * This function mounts the FAT file system on the given base path.
 *
 * @param base_path The base path where the file system should be mounted (e.g., "/sdcard").
 * @return sf_err_t SF_OK on success, SF_FAIL on failure.
 */
sf_err_t sf_file_init_fs(const char* base_path);

/**
 * @brief Deinitializes the file system on the specified base path.
 *
 * This function unmounts the FAT file system from the given base path.
 *
 * @param base_path The base path where the file system is mounted (e.g., "/sdcard").
 * @return sf_err_t SF_OK on success, SF_FAIL on failure.
 */
sf_err_t sf_file_deinit_fs(const char* base_path);

#endif // SF_FILES_H
