/* luaromfs.c C implementation of the luaromfs library.
 *
 * Author: chris.smith@oozlum.co.uk
 * Copyright: (c) 2022 Oozlum
 * Licence: MIT
 */
#include <stdlib.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "romfs.h"
#include "luaromfs.h"

#include ".lua_src.c"

/* Lua C function.  Takes a ROM blob the stack and returns the string representation
 * of the ROM filesystem.  This function must be called for a ROM blob before
 * calling extract_romfile.
 * Stack index 1: ROM string blob
 */
static int c_mount_rom(lua_State *L)
{
  const char *rom_blob, *romfs, *passphrase;
  size_t rom_blob_len, romfs_len;

  rom_blob = luaL_checklstring(L, 1, &rom_blob_len);
  passphrase = luaL_optstring(L, 2, 0);
  romfs = mount_rom(rom_blob, rom_blob_len, &romfs_len, passphrase);
  lua_settop(L, 0);

  if (romfs)
  {
    lua_pushlstring(L, romfs, romfs_len);
    free((void*)romfs);
  }
  else
    lua_pushnil(L);

  return 1;
}

/* Lua C function.  Takes a ROM content and filename on the stack and returns the
 * file contents or nil.
 * Stack index 1: ROM content
 * Stack index 2: filename
 */
static int c_extract_romfile(lua_State *L)
{
  const char *rom, *file, *file_content;
  size_t file_size;

  rom = luaL_checkstring(L, 1);
  file = luaL_checkstring(L, 2);
  file_content = extract_rom_file(rom, file, &file_size);
  lua_settop(L, 0);

  if (file_content)
    lua_pushlstring(L, file_content, file_size);
  else
    lua_pushnil(L);

  return 1;
}

/* lua module entry. */
static int luaopen_luaromfs(lua_State *L)
{
  const char *rom, *bootcode;
  size_t src_len;
  int ok;

  /* load and run the bootstrap from ROM. */
  ok = 0;
  rom = mount_rom(lua_src, lua_src_len, &src_len, 0);
  bootcode = extract_rom_file(rom, "bootstrap.lua", 0);
  if (bootcode)
  {
    if (luaL_loadstring(L, bootcode) == 0)
    {
      lua_pushcclosure(L, c_mount_rom, 0);
      lua_pushcclosure(L, c_extract_romfile, 0);
      lua_call(L, 2, 1);
      ok = 1;
    }
  }
  else
    lua_pushstring(L, "Failed to load bootstrap ROM!");

  free((void*)rom);
  if (!ok)
    lua_error(L);

  return 1;
}

/* load the internal romfs library into the given Lua state. */
void luaromfs_require(lua_State *L)
{
  luaL_requiref(L, "luaromfs", luaopen_luaromfs, 0);
  lua_pop(L, 1);
}

/* mount the given rom blob inside the Lua state. */
void luaromfs_mount(lua_State *L, const char *rom, const size_t rom_len, const char *passphrase)
{
  static const char *script = "local romfs = require'luaromfs'; romfs.mount_string(...)";
  int nargs = 1;

  if (rom && rom_len && luaL_loadstring(L, script) == LUA_OK)
  {
    lua_pushlstring(L, rom, rom_len);
    if (passphrase)
    {
      lua_pushstring(L, passphrase);
      ++nargs;
    }
    lua_call(L, nargs, 0);
  }
}

