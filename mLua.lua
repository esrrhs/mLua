local core = require "libmluacore"

local core_index_cpp_table = core.index_cpp_table
local len = core.len
local core_nextkey = core.nextkey
local core_table_to_cpp = core.table_to_cpp
local core_dump_cpp_table = core.dump_cpp_table

local meta = {}

function meta:__index(key)
    local obj = self.__obj
    local v = core_index_cpp_table(obj, key)
    if type(v) == "userdata" then
        local holder = { __obj = v }
        setmetatable(holder, meta)
        self[key] = holder
        return holder
    else
        return v
    end
end

function meta:__len()
    return len(self.__obj)
end

local function mlua_ipairs(self, index)
    local obj = self.__obj
    index = index + 1
    local value = rawget(self, index)
    if value then
        return index, value
    end
    local sz = len(obj)
    if sz < index then
        return
    end
    return index, self[index]
end

function meta:__ipairs()
    return mlua_ipairs, self, 0
end

local function mlua_next(obj, key)
    local nextkey = core_nextkey(obj.__obj, key)
    if nextkey then
        return nextkey, obj[nextkey]
    end
end

function meta:__pairs()
    return mlua_next, self, nil
end

function _G.table_to_cpp(name, t)
    local light_userdata = core_table_to_cpp(name, t)
    if not light_userdata then
        return nil
    end

    local holder = { __obj = light_userdata }
    setmetatable(holder, meta)
    return holder
end

function _G.dump_cpp_table(name)
    return core_dump_cpp_table(name)
end
