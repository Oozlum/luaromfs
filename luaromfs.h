/* luaromfs.h C implementation of the luaromfs library.
 *
 * Author: chris.smith@oozlum.co.uk
 * Copyright: (c) 2022 Oozlum
 * Licence: MIT
 */
#ifndef LUAROMFS_H
#define LUAROMFS_H

/* load the internal romfs library into the given Lua state. */
void luaromfs_require(lua_State *L);

/* mount the given rom blob inside the Lua state. */
void luaromfs_mount(lua_State *L, const char *rom, const size_t rom_len, const char *passphrase);

#endif
