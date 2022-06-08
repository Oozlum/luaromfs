local romfs = require'luaromfs'

print([[This is internal_rom_src/foo/init.lua, which was embedded into the host application as a static C array.
This file was found and executed by Lua as a result of a call to require'foo'.

I will now attempt to mount from disk the file 'rom.bin' which should have been encrypted with the key 'MySecretKey'...
]])

local ok, err = romfs.mount("./rom.bin", "MySecretKey")
if not ok then
  print(("Failed to mount rom.bin: %s"):format(err))
else
  print("Mount succeeded.  I will now try to require'bar'...\n")
  require'bar'
end
