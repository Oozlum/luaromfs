/* mkrom.c A utility to generate a ROM file from a directory of files
 *
 * Author: chris.smith@oozlum.co.uk
 * Copyright: (c) 2022 Oozlum
 * Licence: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <zlib.h>
#include "sha256.h"
#include "aes.h"

#define DEBUG(...) fprintf(stderr, __VA_ARGS__)

typedef struct _Archive
{
  enum {
    BinaryArchive = 0,
    CArchive
  }
    type;

  const char *c_var;
  int compress;
  FILE *output;
  int line_length;

  char *passphrase;
  int include_passphrase;
  int declare_static;

  char *buffer;
  size_t buffer_end;
  size_t buffer_len;
}
  Archive;

static int compress_buffer(Archive *archive)
{
  z_stream strm;
  char *compressed;
  size_t compressed_len;

  compressed_len = compressBound(archive->buffer_len);
  compressed = (char*)malloc(compressed_len);
  if (!compressed)
  {
    DEBUG("Unable to allocate memory.\n");
    return 0;
  }

  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = archive->buffer_len;
  strm.next_in = (unsigned char*)archive->buffer;
  strm.avail_out = compressed_len;
  strm.next_out = (unsigned char*)compressed;

  if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK)
  {
    DEBUG("deflateInit error.\n");
    free(compressed);
    return 0;
  }

  if (deflate(&strm, Z_FINISH) != Z_STREAM_END)
  {
    deflateEnd(&strm);
    free(compressed);
    DEBUG("deflate error.\n");
    return 0;
  }

  archive->buffer_len = compressed_len - strm.avail_out;
  free(archive->buffer);
  archive->buffer = compressed;
  deflateEnd(&strm);

  return 1;
}

/* initialisation vector for AES. */
static const uint8_t iv[]  = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };

static int encrypt_buffer(Archive *archive)
{
  uint8_t key[SHA256_BLOCK_SIZE];
  SHA256_CTX sha_ctx;
  struct AES_ctx aes_ctx;
  size_t encrypted_len;
  uint8_t *encrypted, pad_byte;

  /* generate the AES key from the passphrase. */
  sha256_init(&sha_ctx);
  sha256_update(&sha_ctx, (uint8_t*)archive->passphrase, strlen(archive->passphrase));
  sha256_final(&sha_ctx, key);

  /* add 16 bytes of guff to the start of the buffer to make it IV independent and
   * pad the buffer to a 16 byte boundary.
   */
  encrypted_len = archive->buffer_len + 16;
  pad_byte = 0x10 - (encrypted_len & 0x0F);
  encrypted_len += pad_byte;
  DEBUG("Archive length: %lu, encrypted length: %lu\n", archive->buffer_len, encrypted_len);

  encrypted = (uint8_t*)malloc(encrypted_len);
  if (!encrypted)
  {
    DEBUG("encrypt_buffer: failed to allocate memory.\n");
    return 0;
  }
  memset(encrypted, pad_byte, encrypted_len);

  /* copy the archive to the encrypt buffer and encrypt it. */
  memcpy(encrypted + 16, archive->buffer, archive->buffer_len);
  AES_init_ctx_iv(&aes_ctx, key, iv);
  AES_CBC_encrypt_buffer(&aes_ctx, encrypted, encrypted_len);

  /* replace the original archive buffer. */
  free(archive->buffer);
  archive->buffer = (char*)encrypted;
  archive->buffer_len = encrypted_len;

  return 1;
}

/* create and open the archive file and write the file header. */
static int open_archive(Archive *archive, const char *file)
{
  if (!archive || !file)
  {
    DEBUG("open_archive: internal error.\n");
    return 0;
  }

  archive->output = fopen(file, "w");
  if (!archive->output)
  {
    perror("open_archive: error opening archive file");
    return 0;
  }

  return 1;
}

static int c_encode_buffer(Archive *archive)
{
  unsigned char c;
  int i, line_length, last_was_hex;
  const char *static_decl = "";

  fprintf(archive->output,
    "/* Auto-generated ROM file, created by mkrom. */\n\n"
    "#include <stddef.h>\n");

  if (archive->declare_static)
    static_decl = "static ";

  if (archive->include_passphrase)
    fprintf(archive->output,
      "%sconst char %s_passphrase[] = \"%s\";\n",
      static_decl, archive->c_var, archive->passphrase);

  fprintf(archive->output,
    "%sconst size_t %s_len = %lu;\n"
    "%sconst char %s[] = ",
    static_decl, archive->c_var, archive->buffer_len + 3,
    static_decl, archive->c_var);

  if (archive->passphrase)
    fwrite("\"ENC\"", 5, 1, archive->output);
  else if (archive->compress)
    fwrite("\"BIN\"", 5, 1, archive->output);
  else
    fwrite("\"ASC\"", 5, 1, archive->output);

  last_was_hex = 0;
  line_length = 0;
  for (i = 0; i != archive->buffer_len; ++i)
  {
    c = archive->buffer[i];

    if (line_length == 0)
    {
      fwrite("\n\"", 2, 1, archive->output);
      line_length = 1;
    }

    if (c == '\\')
    {
      fwrite("\\\\", 2, 1, archive->output);
      line_length += 2;
      last_was_hex = 0;
    }
    else if (c == '\t')
    {
      fwrite("\\t", 2, 1, archive->output);
      line_length += 2;
      last_was_hex = 0;
    }
    else if (c == '"')
    {
      fwrite("\\\"", 2, 1, archive->output);
      line_length += 2;
      last_was_hex = 0;
    }
    else if (isprint(c))
    {
      if (last_was_hex)
      {
        fwrite("\"\"", 2, 1, archive->output);
        line_length += 2;
      }
      fwrite(&c, 1, 1, archive->output);
      ++line_length;
      last_was_hex = 0;
    }
    else
    {
      fprintf(archive->output, "\\x%02X", c);
      line_length += 4;
      last_was_hex = 1;
    }

    if (line_length >= 79)
    {
      fwrite("\"", 1, 1, archive->output);
      line_length = 0;
      last_was_hex = 0;
    }
  }

  if (line_length)
    fwrite("\"", 1, 1, archive->output);
  fwrite(";", 1, 1, archive->output);

  return 1;
}

/* terminate the archive buffer, optionally compress it and encode it as a C
 * array and write it to disk.
 */
static void write_archive(Archive *archive)
{
  /* reallocate memory and write the null header. */
  archive->buffer_len += 5; /* header size */
  archive->buffer = realloc(archive->buffer, archive->buffer_len);
  if (!archive->buffer)
  {
    DEBUG("Error allocating memory.\n");
    return;
  }

  memset(archive->buffer + archive->buffer_end, 0, 5);
  archive->buffer_end += 5;

  if (archive->compress)
  {
    if (!compress_buffer(archive))
    {
      DEBUG("Error compressing buffer.\n");
      return;
    }
  }

  if (archive->passphrase)
  {
    if (!encrypt_buffer(archive))
    {
      DEBUG("Error encrypting buffer.\n");
      return;
    }
  }

  if (archive->type == CArchive)
    c_encode_buffer(archive);
  else
  {
    if (archive->passphrase)
      fwrite("ENC", 3, 1, archive->output);
    else if (archive->compress)
      fwrite("BIN", 3, 1, archive->output);
    else
      fwrite("ASC", 3, 1, archive->output);

    fwrite(archive->buffer, archive->buffer_len, 1, archive->output);
  }

  fclose(archive->output);
}

static int encode_file(Archive *archive, int fd, const char *path, unsigned int path_len, unsigned int prefix_len)
{
  int read_len;
  size_t file_size_pos, file_size;

  /* adjust the path and path_len to ignore the prefix. */
  path += prefix_len;
  path_len = path_len - prefix_len + 1; /* allow for null terminator. */

  /* reallocate memory and write the header. */
  archive->buffer_len += path_len + 5; /* header size */
  archive->buffer = realloc(archive->buffer, archive->buffer_len);
  if (!archive->buffer)
  {
    DEBUG("\nError allocating memory.\n");
    return 0;
  }

  /* store the file size pointer and increment the buffer end. */
  file_size_pos = archive->buffer_end;
  archive->buffer_end += 4;

  /* store the path and path length. */
  archive->buffer[archive->buffer_end++] =  path_len & 0x00FF;
  memcpy(archive->buffer + archive->buffer_end, path, path_len);
  archive->buffer_end += path_len;

  /* read the file in 512 byte chunks until EOF. */
  read_len = 0;
  file_size = 0;
  do
  {
    file_size += read_len;
    archive->buffer = realloc(archive->buffer, archive->buffer_len + file_size + 512);
    if (!archive->buffer)
    {
      DEBUG("\nError allocating memory.\n");
      return 0;
    }
    read_len = read(fd, archive->buffer + archive->buffer_end + file_size, 512);
  }
  while (read_len > 0);

  if (read_len < 0)
  {
    perror("\nencode_file: error reading file");
    return 0;
  }

  DEBUG(" (%lu bytes).\n", file_size);

  /* terminate the file, store the file size and release unused memory. */
  archive->buffer[archive->buffer_end + file_size++] = 0;

  archive->buffer[file_size_pos++] = (file_size >> 24) & 0x000000FF;
  archive->buffer[file_size_pos++] = (file_size >> 16) & 0x000000FF;
  archive->buffer[file_size_pos++] = (file_size >>  8) & 0x000000FF;
  archive->buffer[file_size_pos++] =  file_size        & 0x000000FF;

  archive->buffer_end += file_size;
  archive->buffer_len += file_size;
  archive->buffer = realloc(archive->buffer, archive->buffer_len);
  if (!archive->buffer)
  {
    DEBUG("Error allocating memory.\n");
    return 0;
  }

  return 1;
}

static int archive_file(Archive *archive, char *path, int prefix_len)
{
  struct stat st;
  int fd;

  fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    perror("archive_file: error opening file");
    return 0;
  }

  if (fstat(fd, &st) == -1)
  {
    perror("archive_file: unable to stat file");
    return 0;
  }

  if (st.st_size > 0x0FFFFFFFFUL)
  {
    DEBUG("archive_file: file %s too big\n", path);
    return 0;
  }

  DEBUG("Archiving file: %s as %s", path, path + prefix_len);
  encode_file(archive, fd, path, strlen(path), prefix_len);

  close(fd);

  return 1;
}

static int archive_dir(Archive *archive, char *root, unsigned int prefix_len)
{
  char path[PATH_MAX];
  size_t path_len;
  DIR *dir;
  struct dirent *ent;
  int ok;

  ok = 1;
  dir = opendir(root);
  if (!dir)
  {
    DEBUG("Error: unable to open directory %s\n", root);
    ok = 0;
  }
  DEBUG("Archiving directory: %s\n", root);

  while (ok && (ent = readdir(dir)))
  {
    if ((strcmp(ent->d_name, ".") == 0) || (strcmp(ent->d_name, "..") == 0))
      continue;

    /* generate the path of the directory entry and archive it. */
    path_len = snprintf(path, sizeof(path), "%s/%s", root, ent->d_name);
    if (path_len >= sizeof(path))
    {
      DEBUG("Error: path of %s/%s too long.\n", root, ent->d_name);
      ok = 0;
    }
    else if (ent->d_type == DT_DIR)
      ok = archive_dir(archive, path, prefix_len);
    else if (ent->d_type == DT_REG)
      ok = archive_file(archive, path, prefix_len);
  }

  if (dir)
    closedir(dir);

  return ok;
}

static int usage(const char *name)
{
  DEBUG("%s [-c var_name [-s] [-p]] [-e passphrase] [-x prefix] <source_dir> <output_file>\nArchive the contents of source_dir as a rom file.  The rom file "
      "may be optionally encrypted (-e) and optionally formatted as a C source file containing a constant array (-c var_name), which may be\n"
      "declared static (-s) and may optionally (-p) declare the passphrase string <var_name>_passphrase.\n", name);
  return 1;
}

int main(int argc, char **argv)
{
  Archive archive;
  unsigned int dir_len, prefix_len, i;
  char *input, *output, *prefix;

  /* strip off the input and output. */
  if (argc < 3)
    return usage(argv[0]);
  input = argv[argc - 2];
  output = argv[argc - 1];
  argc -= 3; /* adjust for progname. */

  memset(&archive, 0, sizeof(archive));
  archive.compress = 1;
  archive.type = BinaryArchive;
  prefix_len = 0;

  /* parse the options. */
  if (argc > 8)
    return usage(argv[0]);

  for (i = 1; i < argc; ++i)
  {
    if (strcmp("-c", argv[i]) == 0 && i + 1 <= argc)
    {
      archive.type = CArchive;
      archive.c_var = argv[++i];
    }
    else if (strcmp("-s", argv[i]) == 0)
      archive.declare_static = 1;
    else if (strcmp("-p", argv[i]) == 0)
      archive.include_passphrase = 1;
    else if (strcmp("-e", argv[i]) == 0 && i + 1 <= argc)
      archive.passphrase = argv[++i];
    else if (strcmp("-x", argv[i]) == 0 && i + 1 <= argc)
    {
      prefix = argv[++i];
      prefix_len = strlen(argv[i]);
    }
    else
      return usage(argv[0]);
  }

  if (archive.type != CArchive && (archive.declare_static || archive.include_passphrase))
    return usage(argv[0]);

  /* if the input is '-' then read a single file from stdin and encode to stdout
   * using the output as the encoded filename.
   */
  if (strcmp("-", input) == 0)
  {
    if (prefix_len > strlen(output))
    {
      DEBUG("Error: prefix (%s) is longer than filename (%s)\n", prefix, output);
      return 1;
    }
    archive.output = stdout;
    DEBUG("Archiving file: %s as %s", output, output + prefix_len);
    encode_file(&archive, STDIN_FILENO, output, strlen(output), prefix_len);
  }
  else
  {
    dir_len = strlen(input);
    if (prefix_len > dir_len)
    {
      DEBUG("Error: prefix (%s) is longer than source directory(%s)\n", prefix, input);
      return 1;
    }

    /* trim a trailing separator from the directory name. */
    if (input[dir_len - 1] == '/')
      input[dir_len - 1] = 0;

    if (!open_archive(&archive, output))
      return 1;

    archive_dir(&archive, input, prefix_len);
  }
  write_archive(&archive);
  free(archive.buffer);
}

