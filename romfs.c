/* romfs.c ROM file access functions.
 *
 * Author: chris.smith@oozlum.co.uk
 * Copyright: (c) 2022 Oozlum
 * Licence: MIT
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
#include "romfs.h"
#include "sha256.h"
#include "aes.h"

#define CHUNK_SIZE (1024UL * 1024UL)

/* Return a dynamically allocated, decompressed ROM filesystem image.
 * return zero on failure.
 */
static const char* inflate_rom(const char *rom_blob, size_t rom_blob_len, size_t *romfs_len)
{
  z_stream strm;
  char *romfs;
  int ret;

  romfs = 0;
  *romfs_len = 0;

  /* initialise the z_stream for inflation. */
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = rom_blob_len;
  strm.next_in = (unsigned char*)rom_blob;
  if (inflateInit(&strm) != Z_OK)
    return 0;

  /* inflate the blob until the stream ends. */
  do
  {
    /* allocate a new chunk. */
    *romfs_len += CHUNK_SIZE;
    strm.next_out = realloc(romfs, *romfs_len);
    if (!strm.next_out)
    {
      inflateEnd(&strm);
      return 0;
    }
    /* adjust the pointers. */
    romfs = (char*)strm.next_out;
    strm.next_out += (*romfs_len - CHUNK_SIZE);
    strm.avail_out = CHUNK_SIZE;

    ret = inflate(&strm, Z_NO_FLUSH);
  }
  while (strm.avail_out == 0 && ret == Z_OK);

  if (ret == Z_STREAM_END)
  {
    /* release any unused memory. */
    *romfs_len -= strm.avail_out;
    strm.next_out = realloc(romfs, *romfs_len);
  }
  else
    strm.next_out = 0;

  if (!strm.next_out)
    free(romfs);
  romfs = (char*)strm.next_out;

  inflateEnd(&strm);
  return romfs;
}

/* initialisation vector for AES. */
static const uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

/* Return a dynamically allocated, decrypted ROM filesystem image.
 * return zero on failure.
 */
static const char* decrypt_rom(const char *rom_blob, size_t rom_blob_len, size_t *romfs_len, const char *passphrase)
{
  uint8_t key[SHA256_BLOCK_SIZE];
  SHA256_CTX sha_ctx;
  struct AES_ctx aes_ctx;
  uint8_t *decrypted;

  /* generate the AES key from the passphrase. */
  sha256_init(&sha_ctx);
  sha256_update(&sha_ctx, (uint8_t*)passphrase, strlen(passphrase));
  sha256_final(&sha_ctx, key);

  /* make a modifiable copy of the encrypted content and decrypt it. */
  decrypted = (uint8_t*)malloc(rom_blob_len);
  if (!decrypted)
    return 0;

  memcpy(decrypted, rom_blob, rom_blob_len);
  AES_init_ctx_iv(&aes_ctx, key, iv);
  AES_CBC_decrypt_buffer(&aes_ctx, decrypted, rom_blob_len);

  /* truncate the padding. */
  rom_blob_len -= decrypted[rom_blob_len - 1];

  /* ignore the first 16 bytes and decompress the rest of the decrypted content. */
  rom_blob = inflate_rom((const char*)decrypted + 16, rom_blob_len - 16, romfs_len);

  /* free the decrypted buffer and return. */
  free(decrypted);

  return rom_blob;
}

typedef struct _ROMHeader {
  char magic[3];
  size_t content_len;
  unsigned char content[];
}
  ROMHeader;

/* create and return a dynamically allocated ROM object using the given content. */
static ROMHeader* create_rom(const char *content, size_t len)
{
  ROMHeader *hdr;

  hdr = (ROMHeader*)malloc(sizeof(ROMHeader) + len);
  if (hdr)
  {
    strncpy(hdr->magic, "ROM", 3);
    hdr->content_len = len;
    memcpy(hdr->content, content, len);
  }

  return hdr;
}

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
const char* mount_rom(const char *rom_blob, size_t rom_blob_len, size_t *romfs_len, const char *passphrase)
{
  ROMHeader *romfs;
  const char *rom_content;

  if (!rom_blob || rom_blob_len < 4 || !romfs_len)
    return 0;

  rom_content = 0;
  if (strncmp("ENC", rom_blob, 3) == 0 && passphrase)
    rom_content = decrypt_rom(rom_blob + 3, rom_blob_len - 3, &rom_blob_len, passphrase);
  else if (strncmp("BIN", rom_blob, 3) == 0)
    rom_content = inflate_rom(rom_blob + 3, rom_blob_len - 3, &rom_blob_len);

  romfs = 0;
  if (rom_content)
  {
    romfs = create_rom(rom_content, rom_blob_len);
    free((void*)rom_content);
  }
  else if (strncmp("ASC", rom_blob, 3) == 0)
    romfs = create_rom(rom_blob + 3, rom_blob_len - 3);

  if (romfs)
    *romfs_len = sizeof(ROMHeader) + romfs->content_len;

  return (const char*)romfs;
}

/* find and return a pointer to the string containing the contents of the file
 * matching the given path.  Store the file length in file_len if given.
 * return zero if the file is not found.
 */
const char* extract_rom_file(const char *romfs, const char *path, size_t *file_len)
{
  ROMHeader *rom;
  size_t file_size, offset;
  size_t path_len;
  const char *content;

  if (!romfs || !path)
    return 0;

  rom = (ROMHeader*)romfs;
  if (strncmp("ROM", rom->magic, 3) != 0)
    return 0;

  for (content = 0, file_size = 1, offset = 0;
      file_size != 0 && !content;
      offset += file_size + path_len)
  {
    file_size = (size_t)rom->content[offset++];
    file_size <<= 8;
    file_size += (size_t)rom->content[offset++];
    file_size <<= 8;
    file_size += (size_t)rom->content[offset++];
    file_size <<= 8;
    file_size += (size_t)rom->content[offset++];
    path_len = (size_t)rom->content[offset++];
    if (path_len && strncmp((const char*)rom->content + offset, path, path_len) == 0)
    {
      content = (const char*)rom->content + offset + path_len;
      if (file_len)
        *file_len = file_size - 1; /* exclude null terminator. */
    }
  }

  return content;
}

