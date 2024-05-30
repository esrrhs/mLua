package.cpath = "../?.so;" .. package.cpath
package.path = "./?.lua;./test/?.lua;../?.lua;" .. package.path

require "mLua"
require "cpp_table_proto"
local serpent = require "serpent"

local function gc()
    -- userdata should call multiple times to release memory
    collectgarbage("collect")
    collectgarbage("collect")
    collectgarbage("collect")
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

local function test_benchmark()
    local all_player = {}
    for player_id = 1000000, 1005000 do
        local res2cnt = {}
        for item_id = 100000, 100200 do
            res2cnt[item_id] = { permanent = 100, timing = 200, all = 300 }
        end
        all_player[player_id] = { cnts = res2cnt }
    end
    gc()
    print("lua memory " .. collectgarbage("count") / 1024 .. "MB")
    os.execute("pause")

    _G.cpp_table_load_proto(_G.CPP_TABLE_PROTO)
    for player_id = 1000000, 1005000 do
        all_player[player_id] = _G.cpp_table_sink("Res2Cnt", all_player[player_id])
    end
    gc()
    print("dump_statistic:" .. serpent.block(_G.cpp_table_dump_statistic()))
    print("cpp memory " .. collectgarbage("count") / 1024 .. "MB")
    os.execute("pause")

    all_player = nil
    gc()
    print("end memory " .. collectgarbage("count") / 1024 .. "MB")
    os.execute("pause")
end

test_get_set()

test_benchmark()
