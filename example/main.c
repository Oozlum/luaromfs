/* A simple example of using the luaromfs library.
 *
 * Author: chris.smith@oozlum.co.uk
 * Copyright: (c) 2022 Oozlum
 * Licence: MIT
 */
#include <stdlib.h>
#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "luaromfs.h"
#include ".internal_rom_src.c"

int main(void)
{
  lua_State *L;

  /* create the Lua state and initialise the base libraries. */
  printf("Starting example...\n");
  L = luaL_newstate();
  if (!L)
    return 0;

  luaL_openlibs(L);
  luaromfs_require(L);

  /* mount the internal ROM and require one of its libraries as an example.
   * rom, rom_len and rom_passphrase are static variables provided by .internal_rom_src.c
   */
  luaromfs_mount(L, rom, rom_len, rom_passphrase);
  if (lua_getglobal(L, "require"))
  {
    lua_pushstring(L, "foo");
    lua_call(L, 1, 0);
  }
  
  lua_close(L);

  return 0;
}
