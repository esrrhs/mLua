#include "cpp_table.h"

namespace cpp_table {

StringHeap gStringHeap;
LuaContainerHolder gLuaContainerHolder;

Container::Container(StringView name, int initial_size) {
    m_name = gStringHeap.Add(name);
    m_buffer.resize(initial_size);
}

Container::~Container() {
    gStringHeap.Remove(m_name);
}

static int cpp_table_create_container(lua_State *L) {
    size_t name_size = 0;
    const char *name = lua_tolstring(L, 1, &name_size);
    int initial_size = lua_tointeger(L, 2);
    if (name_size == 0 || initial_size <= 0) {
        luaL_error(L, "cpp_table_create_container: invalid name %s or initial_size %d", name, initial_size);
        return 0;
    }
    auto container = MakeShared<Container>(StringView(name, name_size), initial_size);
    auto pointer = lua_newuserdata(L, 0);
    gLuaContainerHolder.Set(pointer, container);
    return 1;
}

static int cpp_table_set_meta_table(lua_State *L) {
    // lua stack: 1: container, 2: meta table
    auto pointer = lua_touserdata(L, 1);
    if (!pointer) {
        luaL_error(L, "cpp_table_set_meta_table: invalid pointer");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_set_meta_table: no container found %p", pointer);
        return 0;
    }
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_pushvalue(L, 2);
    lua_setmetatable(L, 1);
    return 0;
}

static int cpp_table_delete_container(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    if (!pointer) {
        luaL_error(L, "cpp_table_delete_container: invalid pointer");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_delete_container: no container found %p", pointer);
        return 0;
    }
    gLuaContainerHolder.Remove(pointer);
    return 0;
}

static int cpp_table_container_get_int32(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_int32: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_int32: no container found %p", pointer);
        return 0;
    }
    auto value = container->Get<int32_t>(idx);
    if (!value) {
        luaL_error(L, "cpp_table_container_get_int32: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushinteger(L, *value);
    return 1;
}

static int cpp_table_container_set_int32(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    int32_t value = lua_tointeger(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_int32: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_int32: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<int32_t>(idx, value);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_int32: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_container_get_uint32(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_uint32: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_uint32: no container found %p", pointer);
        return 0;
    }
    auto value = container->Get<uint32_t>(idx);
    if (!value) {
        luaL_error(L, "cpp_table_container_get_uint32: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushinteger(L, *value);
    return 1;
}

static int cpp_table_container_set_uint32(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    uint32_t value = lua_tointeger(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_uint32: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_uint32: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<uint32_t>(idx, value);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_uint32: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_container_get_int64(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_int64: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_int64: no container found %p", pointer);
        return 0;
    }
    auto value = container->Get<int64_t>(idx);
    if (!value) {
        luaL_error(L, "cpp_table_container_get_int64: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushinteger(L, *value);
    return 1;
}

static int cpp_table_container_set_int64(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    int64_t value = lua_tointeger(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_int64: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_int64: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<int64_t>(idx, value);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_int64: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_container_get_uint64(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_uint64: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_uint64: no container found %p", pointer);
        return 0;
    }
    auto value = container->Get<uint64_t>(idx);
    if (!value) {
        luaL_error(L, "cpp_table_container_get_uint64: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushinteger(L, *value);
    return 1;
}

static int cpp_table_container_set_uint64(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    uint64_t value = lua_tointeger(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_uint64: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_uint64: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<uint64_t>(idx, value);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_uint64: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_container_get_float(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_float: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_float: no container found %p", pointer);
        return 0;
    }
    auto value = container->Get<float>(idx);
    if (!value) {
        luaL_error(L, "cpp_table_container_get_float: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushnumber(L, *value);
    return 1;
}

static int cpp_table_container_set_float(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    float value = lua_tonumber(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_float: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_float: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<float>(idx, value);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_float: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_container_get_double(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_double: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_double: no container found %p", pointer);
        return 0;
    }
    auto value = container->Get<double>(idx);
    if (!value) {
        luaL_error(L, "cpp_table_container_get_double: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushnumber(L, *value);
    return 1;
}

static int cpp_table_container_set_double(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    double value = lua_tonumber(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_double: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_double: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<double>(idx, value);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_double: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_container_get_bool(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_bool: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_bool: no container found %p", pointer);
        return 0;
    }
    auto value = container->Get<bool>(idx);
    if (!value) {
        luaL_error(L, "cpp_table_container_get_bool: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, *value);
    return 1;
}

static int cpp_table_container_set_bool(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    bool value = lua_toboolean(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_bool: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_bool: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<bool>(idx, value);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_bool: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

}

std::vector<luaL_Reg> GetCppTableFuncs() {
    return {
            {"cpp_table_create_container",     cpp_table::cpp_table_create_container},
            {"cpp_table_set_meta_table",       cpp_table::cpp_table_set_meta_table},
            {"cpp_table_delete_container",     cpp_table::cpp_table_delete_container},
            {"cpp_table_container_get_int32",  cpp_table::cpp_table_container_get_int32},
            {"cpp_table_container_set_int32",  cpp_table::cpp_table_container_set_int32},
            {"cpp_table_container_get_uint32", cpp_table::cpp_table_container_get_uint32},
            {"cpp_table_container_set_uint32", cpp_table::cpp_table_container_set_uint32},
            {"cpp_table_container_get_int64",  cpp_table::cpp_table_container_get_int64},
            {"cpp_table_container_set_int64",  cpp_table::cpp_table_container_set_int64},
            {"cpp_table_container_get_uint64", cpp_table::cpp_table_container_get_uint64},
            {"cpp_table_container_set_uint64", cpp_table::cpp_table_container_set_uint64},
            {"cpp_table_container_get_float",  cpp_table::cpp_table_container_get_float},
            {"cpp_table_container_set_float",  cpp_table::cpp_table_container_set_float},
            {"cpp_table_container_get_double", cpp_table::cpp_table_container_get_double},
            {"cpp_table_container_set_double", cpp_table::cpp_table_container_set_double},
            {"cpp_table_container_get_bool",   cpp_table::cpp_table_container_get_bool},
            {"cpp_table_container_set_bool",   cpp_table::cpp_table_container_set_bool},
    };
}
