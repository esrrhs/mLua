package.cpath = "../?.so;" .. package.cpath
package.path = "./?.lua;./test/?.lua;../?.lua;" .. package.path

require "mLua"
require "cpp_table_proto"
local serpent = require "serpent"

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

local item1 = {
    id = 100000000001,
    name = "item1",
    price = 100,
}

local item2 = {
    id = 100000000002,
    name = "item2",
    price = 200,
}

local cpptable1 = _G.cpp_table_sink("Item", item1)
local cpptable2 = _G.cpp_table_sink("Item", item2)

cpptable1 = nil
collectgarbage("collect")
print("---------step 1 done---------")

cpptable2 = nil
collectgarbage("collect")
print("---------step 2 done---------")
