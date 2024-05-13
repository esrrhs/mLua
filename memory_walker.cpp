#include "memory_walker.h"

namespace memory_walker {

roaring::Roaring64Map gRoaring64Map;

static int roaring64map_add(lua_State *L) {
    uint64_t value = 0;
    int type = lua_type(L, 1);
    if (type == LUA_TNUMBER) {
        value = lua_tointeger(L, 1);
    } else if (type == LUA_TTABLE) {
        value = (uint64_t) lua_topointer(L, 1);
    } else {
        return 0;
    }
    gRoaring64Map.add(value);
    lua_pushboolean(L, 1);
    return 1;
}

static int roaring64map_addchecked(lua_State *L) {
    uint64_t value = 0;
    int type = lua_type(L, 1);
    if (type == LUA_TNUMBER) {
        value = lua_tointeger(L, 1);
    } else if (type == LUA_TTABLE) {
        value = (uint64_t) lua_topointer(L, 1);
    } else {
        return 0;
    }
    auto ret = gRoaring64Map.addChecked(value);
    lua_pushboolean(L, (int) ret);
    return 1;
}

static int roaring64map_remove(lua_State *L) {
    uint64_t value = 0;
    int type = lua_type(L, 1);
    if (type == LUA_TNUMBER) {
        value = lua_tointeger(L, 1);
    } else if (type == LUA_TTABLE) {
        value = (uint64_t) lua_topointer(L, 1);
    } else {
        return 0;
    }
    gRoaring64Map.remove(value);
    lua_pushboolean(L, 1);
    return 1;
}

static int roaring64map_removechecked(lua_State *L) {
    uint64_t value = 0;
    int type = lua_type(L, 1);
    if (type == LUA_TNUMBER) {
        value = lua_tointeger(L, 1);
    } else if (type == LUA_TTABLE) {
        value = (uint64_t) lua_topointer(L, 1);
    } else {
        return 0;
    }
    auto ret = gRoaring64Map.removeChecked(value);
    lua_pushboolean(L, (int) ret);
    return 1;
}

static int roaring64map_contains(lua_State *L) {
    uint64_t value = 0;
    int type = lua_type(L, 1);
    if (type == LUA_TNUMBER) {
        value = lua_tointeger(L, 1);
    } else if (type == LUA_TTABLE) {
        value = (uint64_t) lua_topointer(L, 1);
    } else {
        return 0;
    }
    lua_pushboolean(L, gRoaring64Map.contains(value));
    return 1;
}

static int roaring64map_clear(lua_State *L) {
    gRoaring64Map.clear();
    return 0;
}

static int roaring64map_maximum(lua_State *L) {
    lua_pushinteger(L, gRoaring64Map.maximum());
    return 1;
}

static int roaring64map_minimum(lua_State *L) {
    lua_pushinteger(L, gRoaring64Map.minimum());
    return 1;
}

static int roaring64map_cardinality(lua_State *L) {
    lua_pushinteger(L, gRoaring64Map.cardinality());
    return 1;
}

static int roaring64map_tostring(lua_State *L) {
    auto ret = gRoaring64Map.toString();
    lua_pushlstring(L, ret.c_str(), ret.size());
    return 1;
}

static int roaring64map_bytesize(lua_State *L) {
    auto ret = gRoaring64Map.getSizeInBytes();
    lua_pushinteger(L, ret);
    return 1;
}

}

std::vector<luaL_Reg> GetMemoryWalkerFuncs() {
    return {
            {"roaring64map_add",           memory_walker::roaring64map_add},
            {"roaring64map_addchecked",    memory_walker::roaring64map_addchecked},
            {"roaring64map_remove",        memory_walker::roaring64map_remove},
            {"roaring64map_removechecked", memory_walker::roaring64map_removechecked},
            {"roaring64map_contains",      memory_walker::roaring64map_contains},
            {"roaring64map_clear",         memory_walker::roaring64map_clear},
            {"roaring64map_maximum",       memory_walker::roaring64map_maximum},
            {"roaring64map_minimum",       memory_walker::roaring64map_minimum},
            {"roaring64map_cardinality",   memory_walker::roaring64map_cardinality},
            {"roaring64map_tostring",      memory_walker::roaring64map_tostring},
            {"roaring64map_bytesize",      memory_walker::roaring64map_bytesize},
    };
}
