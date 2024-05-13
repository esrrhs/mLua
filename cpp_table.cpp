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

}

std::vector<luaL_Reg> GetCppTableFuncs() {
    return {
            {"cpp_table_create_container",    cpp_table::cpp_table_create_container},
            {"cpp_table_set_meta_table",      cpp_table::cpp_table_set_meta_table},
            {"cpp_table_delete_container",    cpp_table::cpp_table_delete_container},
            {"cpp_table_container_get_int32", cpp_table::cpp_table_container_get_int32},
            {"cpp_table_container_set_int32", cpp_table::cpp_table_container_set_int32},
    };
}
