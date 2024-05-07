local core = require "libmluacore"

local core_index_cpp_table = core.index_cpp_table
local core_len_cpp_table = core.len_cpp_table
local core_nextkey_cpp_table = core.nextkey_cpp_table
local core_table_to_cpp = core.table_to_cpp
local core_dump_cpp_table = core.dump_cpp_table

local core_roaring64map_add = core.roaring64map_add
local core_roaring64map_addchecked = core.roaring64map_addchecked
local core_roaring64map_cardinality = core.roaring64map_cardinality
local core_roaring64map_maximum = core.roaring64map_maximum
local core_roaring64map_minimum = core.roaring64map_minimum
local core_roaring64map_bytesize = core.roaring64map_bytesize
local core_roaring64map_clear = core.roaring64map_clear

local core_quick_archiver_save = core.quick_archiver_save
local core_quick_archiver_load = core.quick_archiver_load
local core_quick_archiver_set_lz_threshold = core.quick_archiver_set_lz_threshold
local core_quick_archiver_set_max_buffer_size = core.quick_archiver_set_max_buffer_size
local core_quick_archiver_set_lz_acceleration = core.quick_archiver_set_lz_acceleration

--------------------------lua2cpp begin-------------------------------------
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

--------------------------lua2cpp end-------------------------------------

--------------------------static-perf-lua begin-------------------------------------

local static_perf_lua = {}

static_perf_lua.MAX_PERF_STACK_SIZE = 64
static_perf_lua.MAX_PERF_FUNC_NAME_SIZE = 127

function static_perf_lua.perf_lua_variant_mem(v, calculate_tb, perf_first)
    local t = type(v)
    if t == "string" then
        return 24 + #v -- sizeof(TString)
    elseif t == "number" then
        return 16 -- sizeof(TValue)
    elseif t == "boolean" then
        return 16 -- sizeof(TValue)
    elseif t == "table" then
        if not perf_first then
            return 16 -- sizeof(TValue)
        end
        if calculate_tb then
            local mem = 56 -- sizeof(Table)
            for k, v in pairs(v) do
                local perf_k_first = (type(k) == "table") and core_roaring64map_addchecked(k) or false
                local perf_v_first = (type(v) == "table") and core_roaring64map_addchecked(v) or false
                mem = mem + static_perf_lua.perf_lua_variant_mem(k, calculate_tb, perf_k_first)
                mem = mem + static_perf_lua.perf_lua_variant_mem(v, calculate_tb, perf_v_first)
            end
            return mem
        else
            return 56 -- sizeof(Table)
        end
    elseif t == "function" then
        return 48 -- sizeof(Closure)
    elseif t == "thread" then
        return 208 -- sizeof(lua_State)
    elseif t == "userdata" then
        return 40 -- sizeof(Udata)
    else
        return 0
    end
end

function static_perf_lua.perf_key_name_string(k)
    local kname
    if type(k) == "string" then
        kname = "'" .. k .. "'"
    else
        kname = tostring(k)
    end
    return kname
end

function static_perf_lua.perf_key_name(ctx, id, k)
    local kname = static_perf_lua.perf_key_name_string(k)

    local id_map = ctx.id_map
    if id_map[kname] then
        kname = kname .. " id_" .. id
        id_map[kname] = id
        return kname
    end

    id_map[kname] = id
    return kname
end

function static_perf_lua.perf_alloc_id(ctx, name)
    local name_map = ctx.name_map
    ctx.cur_id = ctx.cur_id + 1
    local cur_id = ctx.cur_id
    name_map[cur_id] = static_perf_lua.perf_key_name(ctx, cur_id, name)
    return cur_id
end

function static_perf_lua.perf_record_cur_use(ctx, cur_id, mem)
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

function static_perf_lua.perf_table_kv(ctx, tb)
    for k, v in pairs(tb) do
        local calculate_tb = false
        if #ctx.callstack == ctx.depth - 1 then
            calculate_tb = true
        end

        local cur_id = static_perf_lua.perf_alloc_id(ctx, k)
        local perf_k_first = (type(k) == "table") and core_roaring64map_addchecked(k) or false
        local perf_v_first = (type(v) == "table") and core_roaring64map_addchecked(v) or false
        local ksz = static_perf_lua.perf_lua_variant_mem(k, calculate_tb, perf_k_first)
        local vsz = static_perf_lua.perf_lua_variant_mem(v, calculate_tb, perf_v_first)
        static_perf_lua.perf_record_cur_use(ctx, cur_id, ksz + vsz)

        if type(v) == "table" and #ctx.callstack < ctx.depth - 1 and perf_v_first then
            local callstack = ctx.callstack
            table.insert(callstack, cur_id)
            static_perf_lua.perf_table_kv(ctx, v)
            table.remove(callstack)
        end
    end
end

function static_perf_lua.perf_write_bin_file(filename, ctx)
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
        for j = 1, static_perf_lua.MAX_PERF_STACK_SIZE do
            file:write(string.pack("i4", callstack[j] or 0))
        end
    end

    local name_map_size = 0
    for id, name in pairs(name_map) do
        if #name > static_perf_lua.MAX_PERF_FUNC_NAME_SIZE then
            name = string.sub(name, 1, static_perf_lua.MAX_PERF_FUNC_NAME_SIZE)
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

---profile a table, calculate the memory usage of the table. and write the profile data to a file. only calculate the first depth table.
---@param filename string
---@param tb table
---@param depth number
function _G.static_perf(filename, tb, depth)
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
        depth = depth or static_perf_lua.MAX_PERF_STACK_SIZE,
    }
    core_roaring64map_clear()
    static_perf_lua.perf_table_kv(ctx, tb)
    static_perf_lua.perf_write_bin_file(filename, ctx)
end

--------------------------static-perf-lua end-------------------------------------

--------------------------memory-walker begin-------------------------------------

local dynamic_perf_lua = {}

function dynamic_perf_lua.memory_walker_log_debug(fmt, ...)
    print(string.format(fmt, ...))
end

function dynamic_perf_lua.memory_walker_log_info(fmt, ...)
    print(string.format(fmt, ...))
end

---open dynamic perf
---@param filename string the output file name
---@param root table mark the root table  eg: { {"package", "a"}, {"package", "b"} } means _G.package.a and _G.package.b
---@param print_level number print level, 0 means print all, N means print first N level
function _G.dynamic_perf_open(filename, root, print_level)
    core_roaring64map_clear()

    dynamic_perf_lua.memory_walker_root = { { "_G" } }
    if root then
        for _, root_info in pairs(root) do
            table.insert(dynamic_perf_lua.memory_walker_root, root_info)
        end
    end

    dynamic_perf_lua.memory_walker_root_index = 1
    dynamic_perf_lua.memory_walker_stack = {}
    table.insert(dynamic_perf_lua.memory_walker_stack, {
        state = "init",
        keys = {},
        key_index = 1,
        cur_size = 0,
    })
    dynamic_perf_lua.memory_walker_total_size = 0
    dynamic_perf_lua.memory_walker_start_time = os.time()

    dynamic_perf_lua.static_perf_ctx = {
        name_map = {},
        id_map = {},
        cur_id = 0,
        profile_data = {}
    }
    dynamic_perf_lua.static_perf_file_name = filename
    dynamic_perf_lua.print_level = print_level or 0

    dynamic_perf_lua.memory_walker_map_is_open = true

    dynamic_perf_lua.memory_walker_log_info("dynamic perf open ok")
end

---close dynamic perf
function _G.dynamic_perf_close()
    static_perf_lua.perf_write_bin_file(dynamic_perf_lua.static_perf_file_name, dynamic_perf_lua.static_perf_ctx)
    core_roaring64map_clear()
    dynamic_perf_lua.memory_walker_root = {}
    dynamic_perf_lua.memory_walker_root_index = 1
    dynamic_perf_lua.memory_walker_stack = {}
    dynamic_perf_lua.memory_walker_total_size = 0
    dynamic_perf_lua.memory_walker_start_time = 0
    dynamic_perf_lua.static_perf_ctx = {}
    dynamic_perf_lua.memory_walker_map_is_open = false
    dynamic_perf_lua.memory_walker_log_info("dynamic perf close ok")
end

---check if dynamic perf is running
function _G.dynamic_perf_running()
    return dynamic_perf_lua.memory_walker_map_is_open
end

function dynamic_perf_lua.calc_size(v)
    local t = type(v)
    if t == "string" then
        if #v <= 40 then
            return 24 -- sizeof(TString)
        else
            return 24 + #v -- sizeof(TString)
        end
    elseif t == "number" then
        return 16 -- sizeof(TValue)
    elseif t == "boolean" then
        return 16 -- sizeof(TValue)
    elseif t == "table" then
        return 56 -- sizeof(Table)
    elseif t == "function" then
        return 48 -- sizeof(Closure)
    elseif t == "thread" then
        return 208 -- sizeof(lua_State)
    elseif t == "userdata" then
        return 40 -- sizeof(Udata)
    else
        return 0
    end
end

-- some root ignore
dynamic_perf_lua.root_ignore = {
    dofile = true,
    package = true,
    next = true,
    xpcall = true,
    type = true,
    pcall = true,
    error = true,
    load = true,
    print = true,
    setmetatable = true,
    select = true,
    pairs = true,
    collectgarbage = true,
    getmetatable = true,
    loadstring = true,
    rawset = true,
    rawget = true,
    unpack = true,
    io = true,
    loadfile = true,
    utf8 = true,
    tonumber = true,
    bit32 = true,
    debug = true,
    require = true,
    math = true,
    string = true,
    os = true,
    table = true,
    module = true,
    _G = true,
    coroutine = true,
    rawlen = true,
    assert = true,
    ipairs = true,
    rawequal = true,
    tostring = true,
    arg = true,
    _VERSION = true,
}

function dynamic_perf_lua.cur_stack_name()
    local root_info = dynamic_perf_lua.memory_walker_root[dynamic_perf_lua.memory_walker_root_index]
    local name = ""
    for _, n in pairs(root_info) do
        name = name .. "." .. n
    end
    for i = 1, #dynamic_perf_lua.memory_walker_stack - 1, 1 do
        name = name .. "." .. tostring(dynamic_perf_lua.memory_walker_stack[i].cur_key)
    end
    return name
end

function dynamic_perf_lua.cur_stack_id()
    local ctx = dynamic_perf_lua.static_perf_ctx
    local root_info = dynamic_perf_lua.memory_walker_root[dynamic_perf_lua.memory_walker_root_index]
    local name = ""
    for _, n in pairs(root_info) do
        name = name .. "." .. n
    end
    local kname = static_perf_lua.perf_key_name_string(name)

    local id_map = ctx.id_map
    local ret = {}
    if not id_map[kname] then
        table.insert(ret, static_perf_lua.perf_alloc_id(ctx, name))
    else
        table.insert(ret, id_map[kname])
    end

    for i = 1, #dynamic_perf_lua.memory_walker_stack, 1 do
        table.insert(ret, dynamic_perf_lua.memory_walker_stack[i].cur_id)
    end

    return ret
end

function dynamic_perf_lua.get_root_table(index)
    local root_info = dynamic_perf_lua.memory_walker_root[index]
    if not root_info then
        return nil
    end
    local t = _G
    for _, name in pairs(root_info) do
        t = t[name]
        if not t then
            return nil
        end
    end
    return t
end

function dynamic_perf_lua.cur_root_table()
    return dynamic_perf_lua.get_root_table(dynamic_perf_lua.memory_walker_root_index)
end

function dynamic_perf_lua.save_callstack_data(size)
    local ctx = dynamic_perf_lua.static_perf_ctx

    local profile_data = ctx.profile_data

    local cur = {
        callstack = dynamic_perf_lua.cur_stack_id(),
        count = size,
    }

    table.insert(profile_data, cur)

    --dynamic_perf_lua.memory_walker_log_debug("memory walker save_callstack_data %s %s %s", dynamic_perf_lua.cur_stack_name(), table.concat(cur.callstack, "."), size)
end

function _G.dynamic_perf_step(max_step_count)
    --dynamic_perf_lua.memory_walker_log_debug("memory walker step begin %s", max_step_count)
    local print_level = dynamic_perf_lua.print_level

    for index, _ in ipairs(dynamic_perf_lua.memory_walker_root) do
        local root = dynamic_perf_lua.get_root_table(index)
        if root then
            core_roaring64map_add(root)
        end
    end

    local table_stack = { dynamic_perf_lua.cur_root_table() }
    local t = table_stack[#table_stack]
    for index = 1, #dynamic_perf_lua.memory_walker_stack - 1, 1 do
        local s = dynamic_perf_lua.memory_walker_stack[index]
        local cur_key = s.cur_key
        local cur = t[cur_key]
        if type(cur) == "table" then
            table.insert(table_stack, cur)
            t = cur
            --dynamic_perf_lua.memory_walker_log_debug("memory walker step table_stack ok %s", cur_key)
        else
            for i = index + 1, #dynamic_perf_lua.memory_walker_stack, 1 do
                dynamic_perf_lua.memory_walker_stack[i] = nil
            end
            --dynamic_perf_lua.memory_walker_log_debug("memory walker step key_index fail %s %s", cur_key, s.key_index)
            break
        end
    end

    local s = dynamic_perf_lua.memory_walker_stack[#dynamic_perf_lua.memory_walker_stack]
    --dynamic_perf_lua.memory_walker_log_debug("memory walker step end, cur state %s %s %s %s %s %s %s", s.state, #s.keys, s.key_index, dynamic_perf_lua.memory_walker_stack[1].cur_size, s.cur_size, #dynamic_perf_lua.memory_walker_stack, cur_stack_name())

    local step_count = 0
    while true do
        local state = s.state
        local keys = s.keys
        if state == "init" then
            local init_key = s.init_key
            --dynamic_perf_lua.memory_walker_log_debug("memory walker start init %s", init_key)
            while true do
                step_count = step_count + 1
                if step_count > max_step_count then
                    s.init_key = init_key
                    --dynamic_perf_lua.memory_walker_log_debug("memory walker init key step max %s", init_key)
                    return
                end

                local ok, next_key = pcall(next, t, init_key)
                if not ok then
                    s.init_key = nil
                    for k in pairs(keys) do
                        keys[k] = nil
                    end
                    s.state = "init"
                    --dynamic_perf_lua.memory_walker_log_debug("memory walker init key step fail %s", init_key)
                    goto continue
                end

                if not next_key then
                    s.state = "walk"
                    s.init_key = nil
                    --dynamic_perf_lua.memory_walker_log_debug("memory walker init key step end %s %s", init_key, step_count)
                    goto continue
                else
                    if dynamic_perf_lua.memory_walker_root_index ~= 1 or #dynamic_perf_lua.memory_walker_stack ~= 1 or not dynamic_perf_lua.root_ignore[next_key] then
                        table.insert(s.keys, next_key)
                        --dynamic_perf_lua.memory_walker_log_debug("memory walker init key step ok %s %s %s", init_key, next_key, step_count)
                    else
                        step_count = step_count - 1
                    end
                    init_key = next_key
                end
            end

        elseif state == "walk" then
            --dynamic_perf_lua.memory_walker_log_debug("memory walker start walk %s", s.key_index)
            local key_index = s.key_index
            local key_index_max = #keys
            local size = s.cur_size
            while true do
                step_count = step_count + 1
                if step_count > max_step_count then
                    s.key_index = key_index
                    s.cur_size = size
                    --dynamic_perf_lua.memory_walker_log_debug("memory walker walk step max %s %s", keys[key_index], size)
                    return
                end

                local key = keys[key_index]
                local value = rawget(t, key)
                key_index = key_index + 1
                if value then
                    size = size + dynamic_perf_lua.calc_size(key) + dynamic_perf_lua.calc_size(value)
                    --dynamic_perf_lua.memory_walker_log_debug("memory walker walk step ok %s %s %s %s", key, calc_size(key) + calc_size(value), size, type(value))

                    if type(value) == "table" then
                        if core_roaring64map_addchecked(value) then
                            --dynamic_perf_lua.memory_walker_log_debug("memory walker walk step next table %s %s %s", key, key_index, size)

                            s.cur_key = key
                            s.key_index = key_index
                            s.cur_size = size

                            s = {
                                state = "init",
                                keys = {},
                                key_index = 1,
                                cur_size = 0,
                                cur_id = static_perf_lua.perf_alloc_id(dynamic_perf_lua.static_perf_ctx, key),
                            }
                            t = value
                            table.insert(dynamic_perf_lua.memory_walker_stack, s)
                            table.insert(table_stack, value)

                            goto continue
                        else
                            --dynamic_perf_lua.memory_walker_log_debug("memory walker walk step next table loop %s %s %s", key, key_index, size)
                        end
                    end
                else
                    --dynamic_perf_lua.memory_walker_log_debug("memory walker walk step no key %s %s", key, size)
                end

                if key_index > key_index_max then
                    --dynamic_perf_lua.memory_walker_log_debug("memory walker walk step end %s %s", key, size)

                    if #dynamic_perf_lua.memory_walker_stack <= print_level or print_level == 0 then
                        dynamic_perf_lua.save_callstack_data(size)
                    end

                    table.remove(dynamic_perf_lua.memory_walker_stack)
                    s = dynamic_perf_lua.memory_walker_stack[#dynamic_perf_lua.memory_walker_stack]
                    local sont = table_stack[#table_stack]
                    table.remove(table_stack)
                    t = table_stack[#table_stack]

                    if not t then
                        dynamic_perf_lua.memory_walker_root_index = dynamic_perf_lua.memory_walker_root_index + 1
                        dynamic_perf_lua.memory_walker_total_size = dynamic_perf_lua.memory_walker_total_size + size
                        if dynamic_perf_lua.memory_walker_root_index > #dynamic_perf_lua.memory_walker_root then
                            dynamic_perf_lua.memory_walker_log_info("memory walker all end size %s cardinality %s max %s min %s bytesize %s usetime %s",
                                    dynamic_perf_lua.memory_walker_total_size, core_roaring64map_cardinality(), core_roaring64map_maximum(),
                                    core_roaring64map_minimum(), core_roaring64map_bytesize(), os.time() - dynamic_perf_lua.memory_walker_start_time)

                            core_roaring64map_clear()

                            _G.dynamic_perf_close()
                        else
                            table.insert(dynamic_perf_lua.memory_walker_stack, {
                                state = "init",
                                keys = {},
                                key_index = 1,
                                cur_size = 0,
                            })
                        end
                        return
                    end

                    --dynamic_perf_lua.memory_walker_log_debug("memory walker walk step next pre %s %s %s", s.cur_key, size, s.cur_size + size)
                    s.cur_size = s.cur_size + size

                    goto continue
                end

            end
        end

        :: continue ::
    end

end

--------------------------memory-walker end-------------------------------------

--------------------------quick-archiver begin-------------------------------------

function _G.quick_archiver_save(t)
    return core_quick_archiver_save(t)
end

function _G.quick_archiver_load(bin)
    return core_quick_archiver_load(bin)
end

function _G.quick_archiver_set_lz_threshold(sz)
    return core_quick_archiver_set_lz_threshold(sz)
end

function _G.quick_archiver_set_max_buffer_size(sz)
    return core_quick_archiver_set_max_buffer_size(sz)
end

function _G.quick_archiver_set_lz_acceleration(sz)
    return core_quick_archiver_set_lz_acceleration(sz)
end

--------------------------quick-archiver end-------------------------------------
