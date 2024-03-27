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
    }
}

_G.memory_walker_open({ { "data", "package", "a" }, { "data", "package", "b" } })

local step = 0
while _G.memory_walker_running() do
    _G.memory_walker_step(1, 2)
    step = step + 1
    print("---------step " .. step .. " done---------")
end
