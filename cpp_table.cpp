#include "cpp_table.h"

namespace cpp_table {

StringHeap gStringHeap;
LuaContainerHolder gLuaContainerHolder;
std::unordered_map<std::string, LayoutPtr> gLayoutMap;

void String::Delete() {
    gStringHeap.Remove(m_str);
    RefCntObj::Delete();
}

Container::Container(LayoutPtr layout) {
    m_layout = layout;
}

Container::~Container() {
}

static int cpp_table_update_layout(lua_State *L) {
    size_t name_size = 0;
    const char *name = lua_tolstring(L, 1, &name_size);
    if (name_size == 0) {
        luaL_error(L, "cpp_table_update_layout: invalid name %s", name);
        return 0;
    }
    luaL_checktype(L, 2, LUA_TTABLE);

    LayoutPtr layout;

    auto layout_key = std::string(name, name_size);
    auto it = gLayoutMap.find(layout_key);
    if (it != gLayoutMap.end()) {
        layout = it->second;
    } else {
        layout = MakeShared<Layout>();
        gLayoutMap[layout_key] = layout;
    }

    std::vector<Layout::Member> member;
    // iterator table {
    //     {
    //         type = v.type,
    //         key = v.key,
    //         value = v.value,
    //         pos = pos,
    //         size = v.size,
    //         tag = v.tag,
    //     }
    // }
    lua_pushnil(L);
    int total_size = 0;
    while (lua_next(L, 2) != 0) {
        Layout::Member mem;
        // name
        if (lua_type(L, -2) != LUA_TSTRING) {
            luaL_error(L, "cpp_table_update_layout: invalid key type %d", lua_type(L, -2));
            return 0;
        }
        size_t name_size = 0;
        const char *name = lua_tolstring(L, -2, &name_size);
        if (name_size == 0) {
            luaL_error(L, "cpp_table_update_layout: invalid key %s", name);
            return 0;
        }
        mem.name.assign(name, name_size);

        if (lua_type(L, -1) != LUA_TTABLE) {
            luaL_error(L, "cpp_table_update_layout: invalid table type %d", lua_type(L, -1));
            return 0;
        }

        // type
        lua_pushstring(L, "type");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TSTRING) {
            luaL_error(L, "cpp_table_update_layout: invalid type type %d", lua_type(L, -1));
            return 0;
        }
        size_t type_size = 0;
        const char *type = lua_tolstring(L, -1, &type_size);
        if (type_size == 0) {
            luaL_error(L, "cpp_table_update_layout: invalid type %s", type);
            return 0;
        }
        mem.type.assign(type, type_size);
        lua_pop(L, 1);

        // key
        lua_pushstring(L, "key");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TSTRING) {
            luaL_error(L, "cpp_table_update_layout: invalid key type %d", lua_type(L, -1));
            return 0;
        }
        size_t key_size = 0;
        const char *key = lua_tolstring(L, -1, &key_size);
        if (key_size == 0) {
            luaL_error(L, "cpp_table_update_layout: invalid key %s", key);
            return 0;
        }
        mem.key.assign(key, key_size);
        lua_pop(L, 1);

        // value
        lua_pushstring(L, "value");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TSTRING) {
            luaL_error(L, "cpp_table_update_layout: invalid value type %d", lua_type(L, -1));
            return 0;
        }
        size_t value_size = 0;
        const char *value = lua_tolstring(L, -1, &value_size);
        mem.value.assign(value, value_size);
        lua_pop(L, 1);

        // pos
        lua_pushstring(L, "pos");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "cpp_table_update_layout: invalid pos type %d", lua_type(L, -1));
            return 0;
        }
        mem.pos = lua_tointeger(L, -1);
        lua_pop(L, 1);

        // size
        lua_pushstring(L, "size");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "cpp_table_update_layout: invalid size type %d", lua_type(L, -1));
            return 0;
        }
        mem.size = lua_tointeger(L, -1);
        lua_pop(L, 1);

        // tag
        lua_pushstring(L, "tag");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "cpp_table_update_layout: invalid tag type %d", lua_type(L, -1));
            return 0;
        }
        mem.tag = lua_tointeger(L, -1);
        lua_pop(L, 1);

        member.push_back(mem);
        lua_pop(L, 1);

        total_size += mem.size;
    }

    layout->SetMember(member);
    layout->SetTotalSize(total_size);
    LLOG("cpp_table_update_layout: %s total size %d", name, total_size);

    return 0;
}

static int cpp_table_create_container(lua_State *L) {
    size_t name_size = 0;
    const char *name = lua_tolstring(L, 1, &name_size);
    if (name_size == 0) {
        luaL_error(L, "cpp_table_create_container: invalid name %s", name);
        return 0;
    }
    auto layout_it = gLayoutMap.find(std::string(name, name_size));
    if (layout_it == gLayoutMap.end()) {
        luaL_error(L, "cpp_table_create_container: no layout found %s", name);
        return 0;
    }
    auto layout = layout_it->second;
    auto container = MakeShared<Container>(layout);
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
    int32_t value = 0;
    bool is_nil = false;
    auto ret = container->Get<int32_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_int32: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, value);
    return 1;
}

static int cpp_table_container_set_int32(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_int32: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    int32_t value = lua_tointeger(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_int32: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_int32: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<int32_t>(idx, value, is_nil);
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
    uint32_t value = 0;
    bool is_nil = false;
    auto ret = container->Get<uint32_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_uint32: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, value);
    return 1;
}

static int cpp_table_container_set_uint32(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_uint32: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    uint32_t value = lua_tointeger(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_uint32: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_uint32: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<uint32_t>(idx, value, is_nil);
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
    int64_t value = 0;
    bool is_nil = false;
    auto ret = container->Get<int64_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_int64: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, value);
    return 1;
}

static int cpp_table_container_set_int64(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_int64: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    int64_t value = lua_tointeger(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_int64: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_int64: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<int64_t>(idx, value, is_nil);
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
    uint64_t value = 0;
    bool is_nil = false;
    auto ret = container->Get<uint64_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_uint64: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, value);
    return 1;
}

static int cpp_table_container_set_uint64(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_uint64: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    uint64_t value = lua_tointeger(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_uint64: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_uint64: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<uint64_t>(idx, value, is_nil);
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
    float value = 0;
    bool is_nil = false;
    auto ret = container->Get<float>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_float: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushnumber(L, value);
    return 1;
}

static int cpp_table_container_set_float(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_float: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    float value = lua_tonumber(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_float: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_float: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<float>(idx, value, is_nil);
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
    double value = 0;
    bool is_nil = false;
    auto ret = container->Get<double>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_double: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushnumber(L, value);
    return 1;
}

static int cpp_table_container_set_double(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_double: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    double value = lua_tonumber(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_double: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_double: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<double>(idx, value, is_nil);
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
    bool value = false;
    bool is_nil = false;
    auto ret = container->Get<bool>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_bool: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushboolean(L, value);
    return 1;
}

static int cpp_table_container_set_bool(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TBOOLEAN && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_bool: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    bool value = lua_toboolean(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_bool: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_bool: no container found %p", pointer);
        return 0;
    }
    auto ret = container->Set<bool>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_bool: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_container_get_string(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_string: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_string: no container found %p", pointer);
        return 0;
    }
    StringPtr value;
    bool is_nil = false;
    auto ret = container->GetSharedObj<String>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_string: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, value->c_str(), value->size());
    return 1;
}

static int cpp_table_container_set_string(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    size_t size = 0;
    if (lua_type(L, 3) != LUA_TSTRING && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_string: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    const char *str = lua_tolstring(L, 3, &size);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_string: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_string: no container found %p", pointer);
        return 0;
    }
    StringPtr shared_str;
    if (!is_nil) {
        shared_str = gStringHeap.Add(HashStringView(str, size));
    }
    auto ret = container->SetSharedObj(idx, shared_str, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_string: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    return 0;
}

}

std::vector<luaL_Reg> GetCppTableFuncs() {
    return {
            {"cpp_table_update_layout",        cpp_table::cpp_table_update_layout},
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
            {"cpp_table_container_get_string", cpp_table::cpp_table_container_get_string},
            {"cpp_table_container_set_string", cpp_table::cpp_table_container_set_string},
    };
}
