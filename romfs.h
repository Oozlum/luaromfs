/* romfs.h ROM file access functions.
 *
 * Author: chris.smith@oozlum.co.uk
 * Copyright: (c) 2022 Oozlum
 * Licence: MIT
 */
#ifndef ROMFS_H
#define ROMFS_H

/* mount and return the filesystem content of a ROM blob.  This must be
 * called on a ROM blob before trying to extract files from it.
 * Store the length of the mounted filesystem in romfs_len.
 * The returned pointer is dynamically allocated and must be free'd by the caller
 * when the ROM is nolonger needed.
 *
 * passphrase may be NULL.
 *
 * return zero on failure.
 */
const char* mount_rom(const char *rom_blob, size_t rom_blob_len, size_t *romfs_len, const char *passphrase);

/* find and return a pointer to the string containing the contents of the file
 * matching the given path.  Store the file length in file_len if given.
 * return zero if the file is not found.
 */
const char* extract_rom_file(const char *romfs, const char *path, size_t *file_len);

#endif

