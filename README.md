# luaromfs
luaromfs comprises:
- a utility `mkrom`, which compresses and optionally encrypts a directory hierarchy of files either as a binary file or a C file containing a static char array and associated metadata variables.
- a C library which extracts files from the ROM blob content.
- a Lua library which exposes the C functionality at a higher level, augmenting the package searchers so that require() finds Lua files within mounted ROM images.

# Getting started
Requirements:
- Lua 5.3 development files (expected in `/usr/include/lua5.3` and `/usr/lib/lua5.3`)
- zlib development files

Clone the repository and run `make`, which will build the library, utility and example, running the example as the last step:
```
# git clone https://github.com/Oozlum/luaromfs.git
# cd luaromfs && make
```
Final output:
```
Starting example...
This is internal_rom_src/foo/init.lua, which was embedded into the host application as a static C array.
This file was found and executed by Lua as a result of a call to require'foo'.

I will now attempt to mount from disk the file 'rom.bin' which should have been encrypted with the key 'MySecretKey'...

Mount succeeded.  I will now try to require'bar'...

This is rom_bin_src/bar/init.lua, which was loaded at runtime from the file rom.bin.
This file was found and executed by Lua as a result of a call to require'bar'.
```
