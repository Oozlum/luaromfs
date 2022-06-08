-- Lua implementation of the luaromfs library.
-- Create and return the ROMFS module
--
-- Author: chris.smith@oozlum.co.uk
-- Copyright: (c) 2022 Oozlum
-- Licence: MIT

local api = {}
api.mount, api.extract = ...

local rom = {}

local M = {
  default_searchpath = '?;?.lua;?/?.lua;?/init.lua',
}

local function rom_extract(r, file)
  if file:sub(1, #r.mount_point) == r.mount_point then
    return api.extract(r.content, file:sub(#r.mount_point + 1))
  end
end

local function mount_string(content, passphrase, mount_point, searchpath)
  searchpath = searchpath or M.default_searchpath or ''
  mount_point = mount_point or ''
  content = api.mount(content, passphrase)
  if not content then
    return nil, 'Mount failed'
  end

  local rom_obj = {
    content = content,
    mount_point = mount_point,
    searchpath = searchpath
  }
  rom[#rom + 1] = rom_obj

  return {
    extract = function(self, file)
      return rom_extract(rom_obj, file)
    end
  }
end
M.mount_string = mount_string

local function mount(file, passphrase, mount_point, searchpath)
  if not file then
    return nil, 'No file specified'
  end
  local f, err = io.open(file, 'r')
  if not f then
    return nil, err
  end
  local rom_content = f:read('*a')
  f:close()
  return mount_string(rom_content, passphrase, mount_point or '', searchpath)
end
M.mount = mount

local function file_not_found(err)
  if err:match('No such file or directory') then
    return true
  end
  return false
end

local function extract(file)
  local content
  for _,r in ipairs(rom) do
    content = rom_extract(r, file)
    if content then
      return content
    end
  end
end

local old_loadfile = loadfile
loadfile = function(file)
  local f, err = old_loadfile(file)
  if not f then
    if file_not_found(err) then
      f = extract(file)
      if f then
        f, err = load(f, file)
      else
        f, err = nil, 'cannot open ' .. tostring(file) .. ': No such file or directory'
      end
    end
  end

  return f, err
end

dofile = function(file)
  local f, err = loadfile(file)
  if not f then
    error(err)
  end
  return f()
end

table.insert(package.searchers, 3, function(modulename)
  local modulepath = string.gsub(modulename, "%.", "/")
  for _,r in ipairs(rom) do
    for path in string.gmatch(r.searchpath, "([^;]+)") do
      local filename = string.gsub(path, "%?", modulepath)
      local file = rom_extract(r, filename)
      if file then
        return load(file, filename)
      end
    end
  end
  return nil
end)

return M

