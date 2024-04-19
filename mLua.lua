local core = require "libmluacore"

--------------------------lua2cpp begin-------------------------------------

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

--------------------------lua2cpp end-------------------------------------

--------------------------perf-lua-table begin-------------------------------------

local MAX_PERF_STACK_SIZE = 64
local MAX_PERF_FUNC_NAME_SIZE = 127

local function perf_lua_variant_mem(v, calculate_tb)
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

--------------------------perf-lua-table end-------------------------------------

--------------------------memory-walker begin-------------------------------------

local core_index_cpp_table = core.index_cpp_table

local core_roaring64map_add = core.roaring64map_add
local core_roaring64map_addchecked = core.roaring64map_addchecked
local core_roaring64map_cardinality = core.roaring64map_cardinality
local core_roaring64map_maximum = core.roaring64map_maximum
local core_roaring64map_minimum = core.roaring64map_minimum
local core_roaring64map_bytesize = core.roaring64map_bytesize
local core_roaring64map_clear = core.roaring64map_clear

_G.memory_walker_map_is_open = _G.memory_walker_map_is_open or false

_G.memory_walker_root = _G.memory_walker_root or {} -- 记录根节点，其实就是_G和所有的文件名
_G.memory_walker_root_index = _G.memory_walker_root_index or 1 -- 记录当前遍历到的根节点
_G.memory_walker_stack = _G.memory_walker_stack or {} -- 记录当前遍历的栈，模拟递归
_G.memory_walker_total_size = _G.memory_walker_total_size or 0 -- 记录总大小
_G.memory_walker_start_time = _G.memory_walker_start_time or 0 -- 记录开始时间

---开启, root是一个table，里面是一些根节点的名字数组。如：root = { {"package", "a"}, {"package", "b"}
function _G.memory_walker_open(root)
    _G.memory_walker_root = { { "_G" } }
    if root then
        for _, root_info in pairs(root) do
            table.insert(_G.memory_walker_root, root_info)
        end
    end

    _G.memory_walker_root_index = 1
    _G.memory_walker_stack = {}
    table.insert(_G.memory_walker_stack, {
        state = "init",
        keys = {},
        key_index = 1,
        cur_size = 0,
    })
    _G.memory_walker_total_size = 0
    _G.memory_walker_start_time = os.time()

    _G.memory_walker_map_is_open = true
    print("memory walker open ok")
end

---停止
function _G.memory_walker_close()
    _G.memory_walker_root = {}
    _G.memory_walker_root_index = 1
    _G.memory_walker_stack = {}
    _G.memory_walker_total_size = 0
    _G.memory_walker_start_time = 0

    _G.memory_walker_map_is_open = false
    print("memory walker close ok")
end

function _G.memory_walker_running()
    return _G.memory_walker_map_is_open
end

local function calc_size(v)
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

-- _G下的一些忽略
local root_ignore = {
    -- 这个是自己的
    memory_walker_map_is_open = true,
    memory_walker_stack = true,
    memory_walker_root = true,
    memory_walker_root_index = true,
    memory_walker_total_size = true,
    memory_walker_start_time = true,

    -- 这个是lua的
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

local function cur_stack_name()
    local root_info = _G.memory_walker_root[_G.memory_walker_root_index]
    local name = ""
    for _, n in pairs(root_info) do
        name = name .. "." .. n
    end
    for i = 1, #_G.memory_walker_stack - 1, 1 do
        name = name .. "." .. tostring(_G.memory_walker_stack[i].cur_key)
    end
    return name
end

local function memory_walker_log_debug(fmt, ...)
    print(string.format(fmt, ...))
end

local function memory_walker_log_info(fmt, ...)
    print(string.format(fmt, ...))
end

local function get_root_table(index)
    local root_info = _G.memory_walker_root[index]
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

local function cur_root_table()
    return get_root_table(_G.memory_walker_root_index)
end

function _G.memory_walker_step(max_step_count, print_level)
    --memory_walker_log_debug("memory walker step begin %s", max_step_count)
    print_level = print_level or 0

    -- 添加一些忽略的根节点
    for index, _ in ipairs(_G.memory_walker_root) do
        local root = get_root_table(index)
        if root then
            core_roaring64map_add(root)
        end
    end

    local table_stack = { cur_root_table() } -- 保存table的栈
    local t = table_stack[#table_stack] -- 当前table
    -- 从根节点一层层往下走，顺便把table放在table_stack中
    for index = 1, #_G.memory_walker_stack - 1, 1 do
        local s = _G.memory_walker_stack[index]
        local cur_key = s.cur_key
        local cur = t[cur_key]
        if type(cur) == "table" then
            table.insert(table_stack, cur)
            t = cur
            --memory_walker_log_debug("memory walker step table_stack ok %s", cur_key)
        else
            -- 如果不是table，删除index之后的元素
            for i = index + 1, #_G.memory_walker_stack, 1 do
                _G.memory_walker_stack[i] = nil
            end
            --memory_walker_log_debug("memory walker step key_index fail %s %s", cur_key, s.key_index)
            break
        end
    end

    local s = _G.memory_walker_stack[#_G.memory_walker_stack]
    --memory_walker_log_debug("memory walker step end, cur state %s %s %s %s %s %s %s", s.state, #s.keys, s.key_index, _G.memory_walker_stack[1].cur_size, s.cur_size, #_G.memory_walker_stack, cur_stack_name())

    local step_count = 0
    while true do
        local state = s.state
        local keys = s.keys
        -- 起步，开始记录所有的key，用来后面遍历，防止计算到一半断了。
        if state == "init" then
            -- 为了防止key太多，这里需要分步。如果分步遍历key中断，那重头开始。
            local init_key = s.init_key
            --memory_walker_log_debug("memory walker start init %s", init_key)
            while true do
                step_count = step_count + 1
                if step_count > max_step_count then
                    -- 超过最大步数，下一帧继续
                    s.init_key = init_key
                    --memory_walker_log_debug("memory walker init key step max %s", init_key)
                    return
                end

                local ok, next_key = pcall(next, t, init_key)
                if not ok then
                    -- key断了，重头开始
                    s.init_key = nil
                    -- 清空keys
                    for k in pairs(keys) do
                        keys[k] = nil
                    end
                    s.state = "init"
                    --memory_walker_log_debug("memory walker init key step fail %s", init_key)
                    goto continue
                end

                -- 继续遍历
                if not next_key then
                    -- 遍历结束，开始下一阶段
                    s.state = "walk"
                    s.init_key = nil
                    --memory_walker_log_debug("memory walker init key step end %s %s", init_key, step_count)
                    goto continue
                else
                    -- 保存key
                    if _G.memory_walker_root_index ~= 1 or #_G.memory_walker_stack ~= 1 or not root_ignore[next_key] then
                        table.insert(s.keys, next_key)
                        --memory_walker_log_debug("memory walker init key step ok %s %s %s", init_key, next_key, step_count)
                    else
                        step_count = step_count - 1
                    end
                    init_key = next_key
                end
            end

        elseif state == "walk" then
            -- 已经集齐了key，开始遍历key
            --memory_walker_log_debug("memory walker start walk %s", s.key_index)
            local key_index = s.key_index
            local key_index_max = #keys
            local size = s.cur_size
            while true do
                step_count = step_count + 1
                if step_count > max_step_count then
                    -- 超过最大步数，下一帧继续
                    s.key_index = key_index
                    s.cur_size = size
                    --memory_walker_log_debug("memory walker walk step max %s %s", keys[key_index], size)
                    return
                end

                local key = keys[key_index]
                local value = rawget(t, key)
                key_index = key_index + 1
                if value then
                    -- 记录大小
                    size = size + calc_size(key) + calc_size(value)
                    --memory_walker_log_debug("memory walker walk step ok %s %s %s %s", key, calc_size(key) + calc_size(value), size, type(value))

                    -- 如果是table，继续往下走
                    if type(value) == "table" then
                        if core_roaring64map_addchecked(value) then
                            --memory_walker_log_debug("memory walker walk step next table %s %s %s", key, key_index, size)

                            s.cur_key = key -- 记录当前key，下次重入时用
                            s.key_index = key_index
                            s.cur_size = size

                            -- 下一层的状态
                            s = {
                                state = "init",
                                keys = {},
                                key_index = 1,
                                cur_size = 0,
                            }
                            t = value
                            table.insert(_G.memory_walker_stack, s)
                            table.insert(table_stack, value)

                            goto continue
                        else
                            -- 有环了，忽略即可
                            --memory_walker_log_debug("memory walker walk step next table loop %s %s %s", key, key_index, size)
                        end
                    end
                else
                    -- 有元素被删除了，忽略即可，这个会导致记录不准确，毕竟是动态的，结果不会差太多。
                    --memory_walker_log_debug("memory walker walk step no key %s %s", key, size)
                end

                -- 遍历结束
                if key_index > key_index_max then
                    --memory_walker_log_debug("memory walker walk step end %s %s", key, size)

                    -- 打印节点
                    if #_G.memory_walker_stack <= print_level then
                        memory_walker_log_info("memory walker memory_walker_stack pop %s size %s", cur_stack_name(), size)
                    end

                    -- 结束，弹出栈
                    table.remove(_G.memory_walker_stack)
                    s = _G.memory_walker_stack[#_G.memory_walker_stack]
                    local sont = table_stack[#table_stack]
                    table.remove(table_stack)
                    t = table_stack[#table_stack]

                    -- 如果栈空了，结束
                    if not t then
                        _G.memory_walker_root_index = _G.memory_walker_root_index + 1
                        _G.memory_walker_total_size = _G.memory_walker_total_size + size
                        if _G.memory_walker_root_index > #_G.memory_walker_root then
                            -- 结束
                            memory_walker_log_info("memory walker all end size %s cardinality %s max %s min %s bytesize %s usetime %s",
                                    _G.memory_walker_total_size, core_roaring64map_cardinality(), core_roaring64map_maximum(),
                                    core_roaring64map_minimum(), core_roaring64map_bytesize(), os.time() - _G.memory_walker_start_time)

                            core_roaring64map_clear()

                            _G.memory_walker_close()
                        else
                            table.insert(_G.memory_walker_stack, {
                                state = "init",
                                keys = {},
                                key_index = 1,
                                cur_size = 0,
                            })
                        end
                        return
                    end

                    -- 累加大小
                    --memory_walker_log_debug("memory walker walk step next pre %s %s %s", s.cur_key, size, s.cur_size + size)
                    s.cur_size = s.cur_size + size

                    goto continue
                end

            end
        end

        :: continue ::
    end

end

--------------------------memory-walker end-------------------------------------
