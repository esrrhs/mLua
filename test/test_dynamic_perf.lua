package.cpath = "../?.so;" .. package.cpath
package.path = "../?.lua;" .. package.path

require "mLua"

_G.data = {
    package = {
        a = { i = 1, s = "123", b = true, array = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } },
        b = { i = 2, s = "123", b = true },
    },
    other_data = {
        a = 1,
        b = 2,
        c = 3,
    },
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

_G.dynamic_perf_open("dynamic_mem.pro", { { "data", "package", "a" }, { "data", "package", "b" } })

local step = 0
while _G.dynamic_perf_running() do
    _G.dynamic_perf_step(1)
    step = step + 1
    print("---------step " .. step .. " done---------")
end
