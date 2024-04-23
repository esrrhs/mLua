package.cpath = "../?.so;" .. package.cpath
package.path = "../?.lua;" .. package.path

local serpent = require "serpent"
require "mLua"

_G.old_data = {
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

_G.quick_archiver_set_lz_threshold(1024)

local bin = _G.quick_archiver_save(_G.old_data)
print("save old data len: ", #bin)

_G.new_data = _G.quick_archiver_load(bin)

function _G.equal(A, B)
    if type(A) ~= "table" or type(B) ~= "table" then
        return A == B
    end

    for k, v in pairs(A) do
        if not _G.equal(v, B[k]) then
            return false
        end
    end

    for k, v in pairs(B) do
        if not _G.equal(v, A[k]) then
            return false
        end
    end

    return true
end

print("is equal: " .. tostring(_G.equal(_G.old_data, _G.new_data)))

_G.old_data = nil
_G.new_data = nil
bin = nil

collectgarbage("collect")
print("init lua mem KB: ", collectgarbage("count"))

_G.data = {
    array = {
    },
    hash = {
    }
}

local function random_value()
    local v = math.random(1, 1000000)
    if v % 6 == 0 then
        return math.random(1, 1000000)
    elseif v % 6 == 1 then
        return "a" .. math.random(1, 1000000)
    elseif v % 6 == 2 then
        return true
    elseif v % 6 == 3 then
        return false
    elseif v % 6 == 4 then
        return math.random(1, 1000000) + 0.1
    elseif v % 6 == 5 then
        return { 1, nil, 2 }
    end
end

for i = 1, 10000 do
    _G.data.array[i] = random_value()
    _G.data.hash["k" .. i] = random_value()
end

collectgarbage("collect")
print("after init data, lua mem KB: ", collectgarbage("count"))

_G.data = _G.quick_archiver_save(_G.data)
print("save data len: ", #_G.data)

collectgarbage("collect")
print("after save data, lua mem KB: ", collectgarbage("count"))

_G.data = _G.quick_archiver_load(_G.data)

collectgarbage("collect")
print("after load data, lua mem KB: ", collectgarbage("count"))
