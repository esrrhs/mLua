package.cpath = "../?.so;" .. package.cpath
package.path = "../?.lua;" .. package.path

require "mLua"
local serpent = require "serpent"

local config = {
    [123] = {
        a = 1.1,
        b = 123123123123,
        c = "abcd",
        e = true,
        f = false,
        g = { 1, 2, 3, 4, 5, 6 },
        h = {
            { a = 1, b = 2, c = 3 },
            { a = 11, b = 22, c = 33 },
            { a = 111, b = 222, c = 333 },
        }
    },
}

local big_config = {}
for i = 0, 10000 do
    big_config[i] = { i = i }
    for j = 0, 1000 do
        big_config[i][j] = j
    end
end

_G.perf_table("mem.pro", config)
