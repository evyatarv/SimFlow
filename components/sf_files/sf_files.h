
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

/**
 * @brief Reads data from a file at a given offset.
 *
 * Opens the file at the specified path, seeks to the given offset, and reads
 * up to @p *size bytes into @p buff. On success, @p *size is updated to the
 * actual number of bytes read.
 *
 * @param file   Path to the file to read from.
 * @param buff   Buffer to store the read data.
 * @param size   [in] Number of bytes requested. [out] Actual bytes read.
 * @param offset Byte offset within the file to start reading from.
 * @return sf_err_t SF_OK on success, SF_FAIL on failure.
 */
sf_err_t sf_file_read(const char* file, uint8_t* buff, uint32_t* size, uint32_t offset);

/**
 * @brief Writes data to a file at a given offset.
 *
 * Opens the file at the specified path, seeks to the given offset, and writes
 * up to @p *size bytes from @p buff. On success, @p *size is updated to the
 * actual number of bytes written. Creates the file if it does not exist.
 *
 * @param file   Path to the file to write to.
 * @param buff   Buffer containing the data to write.
 * @param size   [in] Number of bytes to write. [out] Actual bytes written.
 * @param offset Byte offset within the file to start writing at.
 * @return sf_err_t SF_OK on success, SF_FAIL on failure.
 */
sf_err_t sf_file_write(const char* file, uint8_t* buff, uint32_t* size, uint32_t offset);

#endif // SF_FILES_H
