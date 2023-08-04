local core = require "libmluacore"

local core_index_cpp_table = core.index_cpp_table
local core_len_cpp_table = core.len_cpp_table
local core_nextkey_cpp_table = core.nextkey_cpp_table
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
    return core_len_cpp_table(self.__obj)
end

local function mlua_ipairs(self, index)
    local obj = self.__obj
    index = index + 1
    local value = rawget(self, index)
    if value then
        return index, value
    end
    local sz = core_len_cpp_table(obj)
    if sz < index then
        return
    end
    return index, self[index]
end

function meta:__ipairs()
    return mlua_ipairs, self, 0
end

local function mlua_next(obj, key)
    local nextkey = core_nextkey_cpp_table(obj.__obj, key)
    if nextkey then
        return nextkey, obj[nextkey]
    end
end

function meta:__pairs()
    return mlua_next, self, nil
end

---transform a lua table to a cpp table
---@param name string
---@param t table
function _G.table_to_cpp(name, t)
    local light_userdata = core_table_to_cpp(name, t)
    if not light_userdata then
        return nil
    end

    local holder = { __obj = light_userdata }
    setmetatable(holder, meta)
    return holder
end

---dump a cpp table to string
---@param name string
function _G.dump_cpp_table(name)
    return core_dump_cpp_table(name)
end

---------------------------------------------------------------

local MAX_PERF_STACK_SIZE = 64
local MAX_PERF_FUNC_NAME_SIZE = 127

local function perf_lua_variant_mem(v, calculate_tb)
    local t = type(v)
    if t == "string" then
        return 24 + #v -- sizeof(TString)
    elseif t == "number" then
        return 16 -- sizeof(TValue)
    elseif t == "boolean" then
        return 16 -- sizeof(TValue)
    elseif t == "table" then
        if calculate_tb then
            local mem = 56 -- sizeof(Table)
            for k, v in pairs(v) do
                mem = mem + perf_lua_variant_mem(k, false)
                mem = mem + perf_lua_variant_mem(v, true)
            end
            return mem
        else
            return 56 -- sizeof(Table)
        end
    else
        error("not support type " .. t)
    end
end

local function perf_key_name(ctx, id, k)
    local kname
    if type(k) == "string" then
        kname = "'" .. k .. "'"
    else
        kname = tostring(k)
    end

    local id_map = ctx.id_map
    if id_map[kname] then
        kname = kname .. " id_" .. id
        id_map[kname] = id
        return kname
    end

    id_map[kname] = id
    return kname
end

local function perf_alloc_id(ctx, name)
    local name_map = ctx.name_map
    ctx.cur_id = ctx.cur_id + 1
    local cur_id = ctx.cur_id
    name_map[cur_id] = perf_key_name(ctx, cur_id, name)
    return cur_id
end

local function perf_record_cur_use(ctx, cur_id, mem)
    local callstack = ctx.callstack

    local profile_data = ctx.profile_data

    local cur = {
        callstack = {},
        count = mem,
    }
    for _, v in ipairs(callstack) do
        table.insert(cur.callstack, v)
    end
    table.insert(cur.callstack, cur_id)

    table.insert(profile_data, cur)
end

local function perf_table_kv(ctx, tb)
    for k, v in pairs(tb) do
        local calculate_tb = false
        if #ctx.callstack == MAX_PERF_STACK_SIZE - 1 then
            calculate_tb = true
        end

        local cur_id = perf_alloc_id(ctx, k)
        local ksz = perf_lua_variant_mem(k, calculate_tb)
        local vsz = perf_lua_variant_mem(v, calculate_tb)
        perf_record_cur_use(ctx, cur_id, ksz + vsz)

        if type(v) == "table" and #ctx.callstack < MAX_PERF_STACK_SIZE - 1 then
            local callstack = ctx.callstack
            table.insert(callstack, cur_id)
            perf_table_kv(ctx, v)
            table.remove(callstack)
        end
    end
end

local function perf_write_bin_file(filename, ctx)
    local profile_data = ctx.profile_data
    local name_map = ctx.name_map

    local file = io.open(filename, "wb")
    if not file then
        error("open file failed " .. filename)
    end

    local profile_data_size = #profile_data
    for i = 1, profile_data_size do
        local cur = profile_data[i]
        local callstack = cur.callstack
        local callstack_size = #callstack
        file:write(string.pack("i4", cur.count))
        file:write(string.pack("i4", callstack_size))
        for j = 1, MAX_PERF_STACK_SIZE do
            file:write(string.pack("i4", callstack[j] or 0))
        end
    end

    local name_map_size = 0
    for id, name in pairs(name_map) do
        if #name > MAX_PERF_FUNC_NAME_SIZE then
            name = string.sub(name, 1, MAX_PERF_FUNC_NAME_SIZE)
        end
        local name_len = #name
        file:write(name)
        file:write(string.pack("i4", name_len))
        file:write(string.pack("i4", id))
        name_map_size = name_map_size + 1
    end

    file:write(string.pack("i4", name_map_size))

    print("flush ok", name_map_size);

    file:close()
end

---profile a table, calculate the memory usage of the table. and write the profile data to a file. only support simple table and not recursive table
---@param filename string
---@param tb table
function _G.perf_table(filename, tb)
    local ctx = {
        name_map = {
            [1] = "root",
        },
        id_map = {
            ["root"] = 1,
        },
        cur_id = 1,
        callstack = { 1 },
        profile_data = {},
    }
    perf_table_kv(ctx, tb)
    perf_write_bin_file(filename, ctx)
end
