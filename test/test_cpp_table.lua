package.cpath = "../?.so;" .. package.cpath
package.path = "./?.lua;./test/?.lua;../?.lua;" .. package.path

require "mLua"
require "cpp_table_proto"
local serpent = require "serpent"

local function gc()
    -- userdata should call multiple times to release memory
    for i = 1, 10 do
        collectgarbage("collect")
    end
end

local function get_os()
    return package.config:sub(1, 1) == "\\" and "win" or "unix"
end

local function pause()
    if get_os() == "win" then
        os.execute("pause")
    else
        print("Press enter to continue...")
        io.read()
    end
end

local function test_get_set()

    local player = {
        name = "jack",
        score = 100,
        is_vip = true,
        experience = 100.2,
        items = {
            { id = 100000000001, name = "item1", price = 100 },
            { id = 100000000002, name = "item2", price = 200 },
        },
        labels = { 1, -2, 3, -4, 5 },
        pet = { name = "dog", age = 2, breed = "poodle", weight = 10.5 },
        friends = {
            jack = { name = "jack", age = 20, email = "jack@email.com" },
            tom = { name = "tom", age = 22, email = "tom@email.com" },
        },
        params = { [101] = 100, [102] = 200, [103] = 300 },
    }

    _G.cpp_table_load_proto(_G.CPP_TABLE_PROTO)

    local cpptable = _G.cpp_table_sink("Player", player)
    print("name " .. cpptable.name)
    cpptable.name = "tom!"
    print("name " .. cpptable.name)

    print("score " .. cpptable.score)
    cpptable.score = 200
    print("score " .. cpptable.score)

    print("is_vip " .. tostring(cpptable.is_vip))
    cpptable.is_vip = false
    print("is_vip " .. tostring(cpptable.is_vip))

    print("experience " .. cpptable.experience)
    cpptable.experience = 200.2
    print("experience " .. cpptable.experience)

    print("items1 name " .. cpptable.items[1].name)
    cpptable.items[1].name = "item3!"
    print("items1 name " .. cpptable.items[1].name)

    print("items2 id " .. cpptable.items[2].id)
    cpptable.items[2].id = 100000000003
    print("items2 id " .. cpptable.items[2].id)

    print("labels4 " .. cpptable.labels[4])
    cpptable.labels[4] = 4
    print("labels4 " .. cpptable.labels[4])

    print("pet name " .. cpptable.pet.name)
    cpptable.pet.name = "cat!"
    print("pet name " .. cpptable.pet.name)

    print("pet breed " .. cpptable.pet.breed)
    cpptable.pet.breed = "bulldog!"
    print("pet breed " .. cpptable.pet.breed)

    print("pet age " .. cpptable.pet.age)
    cpptable.pet.age = 3
    print("pet age " .. cpptable.pet.age)

    print("pet weight " .. cpptable.pet.weight)
    cpptable.pet.weight = 11.5
    print("pet weight " .. cpptable.pet.weight)

    print("friend jack name " .. cpptable.friends.jack.name)
    cpptable.friends.jack.name = "jacky!"
    print("friend jack name " .. cpptable.friends.jack.name)

    print("friend tom age " .. cpptable.friends["tom"].age)
    cpptable.friends["tom"].age = 23
    print("friend tom age " .. cpptable.friends["tom"].age)

    print("params101 " .. cpptable.params[101])
    cpptable.params[101] = 101
    print("params101 " .. cpptable.params[101])

    ------------------------------------------
    cpptable = nil
    gc()
end

local function test_benchmark_lua_simple()
    print("start test_benchmark_lua_simple")
    local all_simple_player = {}
    for player_id = 1000000, 1050000 do
        all_simple_player[player_id] = {
            a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8, i = 9, j = 10, k = 11, l = 12, m = 13, n = 14, o = 15, p = 16, q = 17, r = 18, s = 19, t = 20, u = 21, v = 22, w = 23, x = 24, y = 25, z = 26,
        }
    end
    gc()
    print("lua memory " .. collectgarbage("count") / 1024 .. "MB")
    pause()
end

local function test_benchmark_cpp_simple()
    print("start test_benchmark_cpp_simple")
    local all_simple_player = {}
    local player_info = {
        a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8, i = 9, j = 10, k = 11, l = 12, m = 13, n = 14, o = 15, p = 16, q = 17, r = 18, s = 19, t = 20, u = 21, v = 22, w = 23, x = 24, y = 25, z = 26,
    }
    for player_id = 1000000, 1050000 do
        all_simple_player[player_id] = _G.cpp_table_sink("SimpleStruct", player_info)
    end

    gc()
    print("dump_statistic:" .. serpent.block(_G.cpp_table_dump_statistic()))
    print("cpp memory " .. collectgarbage("count") / 1024 .. "MB")
    pause()
end

local function test_benchmark_lua_map()
    print("start test_benchmark_lua_map")
    local player = {
        params = { },
    }
    for i = 1, 10000000 do
        player.params[math.random(10000000, 100000000)] = i
    end
    gc()
    print("lua memory " .. collectgarbage("count") / 1024 .. "MB")
    pause()
end

local function test_benchmark_cpp_map()
    print("start test_benchmark_cpp_map")
    local player = {
        params = { },
    }
    for i = 1, 10000000 do
        player.params[math.random(10000000, 100000000)] = i
    end
    player = _G.cpp_table_sink("Player", player)
    gc()
    print("dump_statistic:" .. serpent.block(_G.cpp_table_dump_statistic()))
    print("cpp memory " .. collectgarbage("count") / 1024 .. "MB")
    pause()
end

local function test_benchmark_lua_array()
    print("start test_benchmark_lua_array")
    local player = {
        labels = { },
    }
    for i = 1, 10000000 do
        table.insert(player.labels, i)
    end
    gc()
    print("lua memory " .. collectgarbage("count") / 1024 .. "MB")
    pause()
end

local function test_benchmark_cpp_array()
    print("start test_benchmark_cpp_array")
    local player = {
        labels = { },
    }
    for i = 1, 10000000 do
        table.insert(player.labels, i)
    end
    player = _G.cpp_table_sink("Player", player)
    gc()
    print("dump_statistic:" .. serpent.block(_G.cpp_table_dump_statistic()))
    print("cpp memory " .. collectgarbage("count") / 1024 .. "MB")
    pause()
end

local function test_benchmark_lua_simple_string()
    print("start test_benchmark_lua_simple_string")
    local all_simple_player = {}
    for player_id = 1000000, 1050000 do
        all_simple_player[player_id] = {
            a = player_id .. "a", b = player_id .. "b", c = player_id .. "c", d = player_id .. "d", e = player_id .. "e", f = player_id .. "f", g = player_id .. "g", h = player_id .. "h", i = player_id .. "i", j = player_id .. "j", k = player_id .. "k", l = player_id .. "l", m = player_id .. "m", n = player_id .. "n", o = player_id .. "o", p = player_id .. "p", q = player_id .. "q", r = player_id .. "r", s = player_id .. "s", t = player_id .. "t", u = player_id .. "u", v = player_id .. "v", w = player_id .. "w", x = player_id .. "x", y = player_id .. "y", z = player_id .. "z",
        }
    end
    gc()
    print("lua memory " .. collectgarbage("count") / 1024 .. "MB")
    pause()
end

local function test_benchmark_cpp_simple_string()
    print("start test_benchmark_cpp_simple_string")
    local all_simple_player = {}
    for player_id = 1000000, 1050000 do
        local player_info = {
            a = player_id .. "a", b = player_id .. "b", c = player_id .. "c", d = player_id .. "d", e = player_id .. "e", f = player_id .. "f", g = player_id .. "g", h = player_id .. "h", i = player_id .. "i", j = player_id .. "j", k = player_id .. "k", l = player_id .. "l", m = player_id .. "m", n = player_id .. "n", o = player_id .. "o", p = player_id .. "p", q = player_id .. "q", r = player_id .. "r", s = player_id .. "s", t = player_id .. "t", u = player_id .. "u", v = player_id .. "v", w = player_id .. "w", x = player_id .. "x", y = player_id .. "y", z = player_id .. "z",
        }
        all_simple_player[player_id] = _G.cpp_table_sink("SimpleStringStruct", player_info)
    end

    gc()
    print("cpp memory " .. collectgarbage("count") / 1024 .. "MB")
    pause()
end

local function test_benchmark_lua_array_string()
    print("start test_benchmark_lua_array_string")
    local player = {
        emails = { },
    }
    for i = 1, 1000000 do
        table.insert(player.emails, "s" .. i)
    end
    gc()
    print("lua memory " .. collectgarbage("count") / 1024 .. "MB")
    pause()
end

local function test_benchmark_cpp_array_string()
    print("start test_benchmark_cpp_array_string")
    local player = {
        emails = { },
    }
    for i = 1, 1000000 do
        table.insert(player.emails, "s" .. i)
    end
    player = _G.cpp_table_sink("Player", player)
    gc()
    print("cpp memory " .. collectgarbage("count") / 1024 .. "MB")
    pause()
end

_G.cpp_table_load_proto(_G.CPP_TABLE_PROTO)

print("Please input test type: ")
print("  1: test_get_set")
print("  2: test_benchmark_lua_simple")
print("  3: test_benchmark_cpp_simple")
print("  4: test_benchmark_lua_map")
print("  5: test_benchmark_cpp_map")
print("  6: test_benchmark_lua_array")
print("  7: test_benchmark_cpp_array")
print("  8: test_benchmark_lua_simple_string")
print("  9: test_benchmark_cpp_simple_string")
print(" 10: test_benchmark_lua_array_string")
print(" 11: test_benchmark_cpp_array_string")

local type = io.read()
while true do

    if type == "1" then
        test_get_set()
        break
    elseif type == "2" then
        test_benchmark_lua_simple()
    elseif type == "3" then
        test_benchmark_cpp_simple()
    elseif type == "4" then
        test_benchmark_lua_map()
    elseif type == "5" then
        test_benchmark_cpp_map()
    elseif type == "6" then
        test_benchmark_lua_array()
    elseif type == "7" then
        test_benchmark_cpp_array()
    elseif type == "8" then
        test_benchmark_lua_simple_string()
    elseif type == "9" then
        test_benchmark_cpp_simple_string()
    elseif type == "10" then
        test_benchmark_lua_array_string()
    elseif type == "11" then
        test_benchmark_cpp_array_string()
    else
        print("Invalid test type")
        break
    end
    print("try again")
end
