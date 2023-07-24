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
local sum = 0
for i = 0, 1000 do
    big_config[i] = { i = i }
    for j = 0, 1000 do
        big_config[i][j] = j
    end
    sum = sum + big_config[i].i
end

print("config[123].a: " .. config[123].a)
print("config[123].c: " .. config[123].c)
print("#config[123].g: " .. #config[123].g)
for k, v in pairs(config[123]) do
    print(string.format("pairs config[123][%s]: %s", k, v))
end
for k, v in ipairs(config[123].g) do
    print("ipairs config[123].g[" .. k .. "]: " .. v)
end
for k, v in pairs(config[123].g) do
    print("pairs config[123].g[" .. k .. "]: " .. v)
end
print("big_config test sum:" .. sum)

collectgarbage("collect")
print("before lua table to cpp, lua memory is " .. collectgarbage("count"))

config = _G.table_to_cpp("config", config)
big_config = _G.table_to_cpp("big_config", big_config)
print("dump config: " .. _G.dump_cpp_table("config"))

collectgarbage("collect")
print("after lua table to cpp, lua memory is " .. collectgarbage("count"))

print("config[123].a: " .. config[123].a)
print("config[123].c: " .. config[123].c)
print("#config[123].g: " .. #config[123].g)
for k, v in pairs(config[123]) do
    print(string.format("pairs config[123][%s]: %s", k, v))
end
for k, v in ipairs(config[123].g) do
    print("ipairs config[123].g[" .. k .. "]: " .. v)
end
for k, v in pairs(config[123].g) do
    print("pairs config[123].g[" .. k .. "]: " .. v)
end
sum = 0
for i = 0, 1000 do
    sum = sum + big_config[i].i
end
print("big_config test sum:" .. sum)

collectgarbage("collect")
print("after use cpp table, lua memory is " .. collectgarbage("count"))
