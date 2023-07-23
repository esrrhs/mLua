require "mLua"

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
end

collectgarbage("collect")
print("before lua table to cpp, lua memory is " .. collectgarbage("count"))

config = _G.table_to_cpp("config", config)
big_config = _G.table_to_cpp("big_config", big_config)
print(_G.dump_cpp_table("config"))

collectgarbage("collect")
print("after lua table to cpp, lua memory is " .. collectgarbage("count"))

print("done")
