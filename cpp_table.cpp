#include "cpp_table.h"

namespace cpp_table {

StringHeap gStringHeap;
LuaContainerHolder gLuaContainerHolder;
LayoutMgr gLayoutMgr;

void String::Delete() {
    gStringHeap.Remove(m_str);
    RefCntObj::Delete();
}

Container::Container(LayoutPtr layout) {
    m_layout = layout;
}

Container::~Container() {
    LLOG("Container::~Container: %s %p", GetName().data(), this);
    ReleaseAllSharedObj();
}

LuaContainerHolder::~LuaContainerHolder() {
    LLOG("LuaContainerHolder::~LuaContainerHolder");
}

void Container::ReleaseAllSharedObj() {
    auto &members = m_layout->GetMember();
    for (auto &it: members) {
        auto mem = it.second;
        if (!mem->shared) {
            continue;
        }
        int pos = mem->pos;
        SharedPtr<RefCntObj> out;
        bool is_nil = false;
        auto ret = GetSharedObj<RefCntObj>(pos, out, is_nil);
        if (!ret) {
            LERR("Container::ReleaseAllSharedObj: %s invalid pos %d", m_layout->GetName()->data(), pos);
            return;
        }
        if (!is_nil) {
            out->Release();
        }
    }
}

Array::Array(Layout::MemberPtr layout_member) {
    m_layout_member = layout_member;
}

Array::~Array() {
    LLOG("Array::~Array: %s %p", GetName().data(), this);
    ReleaseAllSharedObj();
}

void Array::ReleaseAllSharedObj() {
    if (!m_layout_member->key_shared) {
        return;
    }
    int element_size = m_buffer.size() / m_layout_member->key_size;
    for (int i = 0; i < element_size; ++i) {
        SharedPtr<RefCntObj> out;
        bool is_nil = false;
        auto ret = GetSharedObj<RefCntObj>(i, out, is_nil);
        if (!ret) {
            LERR("Array::ReleaseAllSharedObj: %s invalid pos %d", m_layout_member->name->data(), i);
            return;
        }
        if (!is_nil) {
            out->Release();
        }
    }
}

Map::Map(Layout::MemberPtr layout_member) {
    m_layout_member = layout_member;
    m_map.m_void = 0;
    ReleaseAllSharedObj();
}

Map::~Map() {
    LLOG("Map::~Map: %s %p", GetName().data(), this);
}

void Map::ReleaseAllSharedObj() {
    // TODO
}

static int cpp_table_set_message_id(lua_State *L) {
    size_t name_size = 0;
    const char *name = lua_tolstring(L, 1, &name_size);
    if (name_size == 0) {
        luaL_error(L, "cpp_table_update_layout: invalid name %s", name);
        return 0;
    }
    int message_id = lua_tointeger(L, 2);
    if (message_id < 0) {
        luaL_error(L, "cpp_table_set_message_id: invalid message_id %d", message_id);
        return 0;
    }
    auto layout_key = gStringHeap.Add(HashStringView(name, name_size));
    gLayoutMgr.SetMessageId(layout_key, message_id);
    return 0;
}

static int cpp_table_update_layout(lua_State *L) {
    size_t name_size = 0;
    const char *name = lua_tolstring(L, 1, &name_size);
    if (name_size == 0) {
        luaL_error(L, "cpp_table_update_layout: invalid name %s", name);
        return 0;
    }
    luaL_checktype(L, 2, LUA_TTABLE);

    auto layout_key = gStringHeap.Add(HashStringView(name, name_size));
    auto layout = gLayoutMgr.GetLayout(layout_key);
    if (!layout) {
        layout = MakeShared<Layout>();
        gLayoutMgr.SetLayout(layout_key, layout);
    }

    // iterator table {
    //     {
    //         type = v.type,
    //         key = v.key,
    //         value = v.value,
    //         pos = pos,
    //         size = v.size,
    //         tag = v.tag,
    //         shared = v.shared,
    //     }
    // }
    lua_pushnil(L);
    int total_size = 0;
    while (lua_next(L, 2) != 0) {
        auto mem = MakeShared<Layout::Member>();
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
        mem->name = gStringHeap.Add(HashStringView(name, name_size));

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
        mem->type = gStringHeap.Add(HashStringView(type, type_size));
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
        mem->key = gStringHeap.Add(HashStringView(key, key_size));
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
        mem->value = gStringHeap.Add(HashStringView(value, value_size));
        lua_pop(L, 1);

        // pos
        lua_pushstring(L, "pos");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "cpp_table_update_layout: invalid pos type %d", lua_type(L, -1));
            return 0;
        }
        mem->pos = lua_tointeger(L, -1);
        lua_pop(L, 1);

        // size
        lua_pushstring(L, "size");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "cpp_table_update_layout: invalid size type %d", lua_type(L, -1));
            return 0;
        }
        mem->size = lua_tointeger(L, -1);
        lua_pop(L, 1);

        // tag
        lua_pushstring(L, "tag");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "cpp_table_update_layout: invalid tag type %d", lua_type(L, -1));
            return 0;
        }
        mem->tag = lua_tointeger(L, -1);
        lua_pop(L, 1);

        // shared
        lua_pushstring(L, "shared");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "cpp_table_update_layout: invalid tag type %d", lua_type(L, -1));
            return 0;
        }
        mem->shared = lua_tointeger(L, -1);
        lua_pop(L, 1);

        // message_id
        lua_pushstring(L, "message_id");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "cpp_table_update_layout: invalid message_id type %d", lua_type(L, -1));
            return 0;
        }
        mem->message_id = lua_tointeger(L, -1);
        lua_pop(L, 1);

        // value_message_id
        lua_pushstring(L, "value_message_id");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "cpp_table_update_layout: invalid value_message_id type %d", lua_type(L, -1));
            return 0;
        }
        mem->value_message_id = lua_tointeger(L, -1);
        lua_pop(L, 1);

        // key_size
        lua_pushstring(L, "key_size");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "cpp_table_update_layout: invalid key_size type %d", lua_type(L, -1));
            return 0;
        }
        mem->key_size = lua_tointeger(L, -1);
        lua_pop(L, 1);

        // key_shared
        lua_pushstring(L, "key_shared");
        lua_gettable(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            luaL_error(L, "cpp_table_update_layout: invalid key_shared type %d", lua_type(L, -1));
            return 0;
        }
        mem->key_shared = lua_tointeger(L, -1);
        lua_pop(L, 1);

        auto old = layout->GetMember(mem->tag);
        if (old) {
            *old = *mem;
        } else {
            layout->SetMember(mem->tag, mem);
        }
        lua_pop(L, 1);

        total_size += mem->size;
    }

    auto message_id = gLayoutMgr.GetMessageId(layout_key);
    if (message_id < 0) {
        luaL_error(L, "cpp_table_update_layout: no message_id found %s", layout_key->data());
        return 0;
    }

    layout->SetMessageId(message_id);
    layout->SetName(layout_key);
    layout->SetTotalSize(total_size);
    LLOG("cpp_table_update_layout: %s total size %d message_id %d", name, total_size, message_id);

    return 0;
}

static void cpp_table_reg_container_userdata(lua_State *L, Container *container) {
    // create weak table _G.CPP_TABLE_CONTAINER, and set container pointer -> userdata pointer mapping
    lua_getglobal(L, "CPP_TABLE_CONTAINER");// stack: 1: userdata, 2: weak table
    if (lua_type(L, -1) != LUA_TTABLE) { // stack: 1: userdata, 2: nil
        lua_pop(L, 1); // stack: 1: userdata
        // create weak table
        lua_newtable(L); // stack: 1: userdata, 2: table
        lua_newtable(L); // stack: 1: userdata, 2: table, 3: table
        lua_pushstring(L, "v"); // stack: 1: userdata, 2: table, 3: table, 4: "v"
        lua_setfield(L, -2, "__mode"); // stack: 1: userdata, 2: table, 3: table(__mode = "v")
        lua_setmetatable(L, -2); // stack: 1: userdata, 2: table
        lua_setglobal(L, "CPP_TABLE_CONTAINER"); // stack: 1: userdata
        lua_getglobal(L, "CPP_TABLE_CONTAINER"); // stack: 1: userdata, 2: weak table
    }
    lua_pushlightuserdata(L, container); // stack: 1: userdata, 2: weak table, 3: pointer
    lua_pushvalue(L, -3); // stack: 1: userdata, 2: weak table, 3: pointer, 4: userdata
    lua_settable(L, -3); // stack: 1: userdata, 2: weak table
    lua_pop(L, 1); // stack: 1: userdata
    // set meta table from _G.CPP_TABLE_LAYOUT_META_TABLE
    lua_getglobal(L, "CPP_TABLE_LAYOUT_META_TABLE");// stack: 1: userdata, 2: global meta
    if (lua_type(L, -1) != LUA_TTABLE) { // stack: 1: userdata, 2: nil
        lua_pop(L, 1); // stack: 1: userdata
        luaL_error(L, "cpp_table_reg_container_userdata: no _G.CPP_TABLE_LAYOUT_META_TABLE found");
        return;
    }
    lua_pushlstring(L, container->GetName().data(),
                    container->GetName().size()); // stack: 1: userdata, 2: global meta, 3: name
    lua_gettable(L, -2); // stack: 1: userdata, 2: global meta, 3: meta
    if (lua_type(L, -1) != LUA_TTABLE) {
        lua_pop(L, 1); // stack: 1: userdata, 2: global meta
        luaL_error(L, "cpp_table_reg_container_userdata: no meta table found %s", container->GetName().data());
        return;
    }
    lua_setmetatable(L, -3); // stack: 1: userdata, 2: global meta
    lua_pop(L, 1); // stack: 1: userdata
}

static bool cpp_table_get_container_userdata(lua_State *L, Container *container) {
    lua_getglobal(L, "CPP_TABLE_CONTAINER");// stack: 1: weak table
    if (lua_type(L, -1) != LUA_TTABLE) { // stack: 1: nil
        lua_pop(L, 1); // stack:
        return false;
    }
    lua_pushlightuserdata(L, container); // stack: 1: weak table, 2: pointer
    lua_gettable(L, -2); // stack: 1: weak table, 2: userdata
    lua_remove(L, -2); // stack: 1: userdata
    if (lua_type(L, -1) != LUA_TUSERDATA) {
        lua_pop(L, 1); // stack:
        return false;
    }
    // stack: 1: userdata
    return true;
}

static void cpp_table_remove_container_userdata(lua_State *L, Container *container) {
    lua_getglobal(L, "CPP_TABLE_CONTAINER");// stack: 1: weak table
    if (lua_type(L, -1) != LUA_TTABLE) { // stack: 1: nil
        lua_pop(L, 1); // stack:
        return;
    }
    lua_pushlightuserdata(L, container); // stack: 1: weak table, 2: pointer
    lua_pushnil(L); // stack: 1: weak table, 2: pointer, 3: nil
    lua_settable(L, -3); // stack: 1: weak table
    lua_pop(L, 1); // stack:
}

static void cpp_table_get_container_push_pointer(lua_State *L, Container *container_pointer) {
    auto find = cpp_table_get_container_userdata(L, container_pointer);
    if (!find) {
        auto userdata_pointer = (void **) lua_newuserdata(L, sizeof(void *));
        *userdata_pointer = container_pointer;
        gLuaContainerHolder.Set(userdata_pointer, container_pointer);
        cpp_table_reg_container_userdata(L, container_pointer);
        LLOG("cpp_table_get_container_push_pointer: %s new %p", container_pointer->GetName().data(), container_pointer);
    } else {
        LLOG("cpp_table_get_container_push_pointer: %s already exist %p", container_pointer->GetName().data(),
             container_pointer);
    }
}

static void cpp_table_reg_array_container_userdata(lua_State *L, Array *array, StringPtr key) {
    // create weak table _G.CPP_TABLE_ARRAY_CONTAINER, and set array pointer -> userdata pointer mapping
    lua_getglobal(L, "CPP_TABLE_ARRAY_CONTAINER");// stack: 1: userdata, 2: weak table
    if (lua_type(L, -1) != LUA_TTABLE) { // stack: 1: userdata, 2: nil
        lua_pop(L, 1); // stack: 1: userdata
        // create weak table
        lua_newtable(L); // stack: 1: userdata, 2: table
        lua_newtable(L); // stack: 1: userdata, 2: table, 3: table
        lua_pushstring(L, "v"); // stack: 1: userdata, 2: table, 3: table, 4: "v"
        lua_setfield(L, -2, "__mode"); // stack: 1: userdata, 2: table, 3: table(__mode = "v")
        lua_setmetatable(L, -2); // stack: 1: userdata, 2: table
        lua_setglobal(L, "CPP_TABLE_ARRAY_CONTAINER"); // stack: 1: userdata
        lua_getglobal(L, "CPP_TABLE_ARRAY_CONTAINER"); // stack: 1: userdata, 2: weak table
    }
    lua_pushlightuserdata(L, array); // stack: 1: userdata, 2: weak table, 3: pointer
    lua_pushvalue(L, -3); // stack: 1: userdata, 2: weak table, 3: pointer, 4: userdata
    lua_settable(L, -3); // stack: 1: userdata, 2: weak table
    lua_pop(L, 1); // stack: 1: userdata
    // set meta table from _G.CPP_TABLE_LAYOUT_META_TABLE
    lua_getglobal(L, "CPP_TABLE_LAYOUT_ARRAY_META_TABLE");// stack: 1: userdata, 2: global meta
    if (lua_type(L, -1) != LUA_TTABLE) { // stack: 1: userdata, 2: nil
        lua_pop(L, 1); // stack: 1: userdata
        luaL_error(L, "cpp_table_reg_container_userdata: no _G.CPP_TABLE_LAYOUT_ARRAY_META_TABLE found");
        return;
    }
    lua_pushlstring(L, key->c_str(), key->size()); // stack: 1: userdata, 2: global meta, 3: name
    lua_gettable(L, -2); // stack: 1: userdata, 2: global meta, 3: meta
    if (lua_type(L, -1) != LUA_TTABLE) {
        lua_pop(L, 1); // stack: 1: userdata, 2: global meta
        luaL_error(L, "cpp_table_reg_container_userdata: no meta table found %s", key->c_str());
        return;
    }
    lua_setmetatable(L, -3); // stack: 1: userdata, 2: global meta
    lua_pop(L, 1); // stack: 1: userdata
}

static bool cpp_table_get_array_container_userdata(lua_State *L, Array *array) {
    lua_getglobal(L, "CPP_TABLE_ARRAY_CONTAINER");// stack: 1: weak table
    if (lua_type(L, -1) != LUA_TTABLE) { // stack: 1: nil
        lua_pop(L, 1); // stack:
        return false;
    }
    lua_pushlightuserdata(L, array); // stack: 1: weak table, 2: pointer
    lua_gettable(L, -2); // stack: 1: weak table, 2: userdata
    lua_remove(L, -2); // stack: 1: userdata
    if (lua_type(L, -1) != LUA_TUSERDATA) {
        lua_pop(L, 1); // stack:
        return false;
    }
    // stack: 1: userdata
    return true;
}

static void cpp_table_remove_array_container_userdata(lua_State *L, Array *array) {
    lua_getglobal(L, "CPP_TABLE_ARRAY_CONTAINER");// stack: 1: weak table
    if (lua_type(L, -1) != LUA_TTABLE) { // stack: 1: nil
        lua_pop(L, 1); // stack:
        return;
    }
    lua_pushlightuserdata(L, array); // stack: 1: weak table, 2: pointer
    lua_pushnil(L); // stack: 1: weak table, 2: pointer, 3: nil
    lua_settable(L, -3); // stack: 1: weak table
    lua_pop(L, 1); // stack:
}

static void
cpp_table_reg_map_container_userdata(lua_State *L, Map *map, StringPtr key, StringPtr value) {

}

static bool cpp_table_get_map_container_userdata(lua_State *L, Map *map) {

}

static int cpp_table_create_container(lua_State *L) {
    size_t name_size = 0;
    const char *name = lua_tolstring(L, 1, &name_size);
    if (name_size == 0) {
        luaL_error(L, "cpp_table_create_container: invalid name %s", name);
        return 0;
    }
    auto layout_key = gStringHeap.Add(HashStringView(name, name_size));
    auto layout = gLayoutMgr.GetLayout(layout_key);
    if (!layout) {
        luaL_error(L, "cpp_table_create_container: no layout found %s", name);
        return 0;
    }
    auto container = MakeShared<Container>(layout);
    auto pointer = (void **) lua_newuserdata(L, sizeof(void *));
    *pointer = container.get();
    gLuaContainerHolder.Set(pointer, container);
    // create container pointer -> userdata pointer mapping in lua _G.CPP_TABLE_CONTAINER which is a weak table
    cpp_table_reg_container_userdata(L, container.get());
    return 1;
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
    cpp_table_remove_container_userdata(L, container.get());
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

static int cpp_table_container_get_obj(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_obj: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_obj: no container found %p", pointer);
        return 0;
    }
    ContainerPtr obj;
    bool is_nil = false;
    auto ret = container->GetSharedObj<Container>(idx, obj, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_obj: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    auto container_pointer = obj.get();
    cpp_table_get_container_push_pointer(L, container_pointer);
    return 1;
}

static int cpp_table_container_set_obj(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TUSERDATA && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_obj: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    void *obj_pointer = lua_touserdata(L, 3);
    bool is_nil = lua_isnil(L, 3);
    int message_id = lua_tointeger(L, 4);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_obj: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_obj: no container found %p", pointer);
        return 0;
    }
    if (!obj_pointer) {
        luaL_error(L, "cpp_table_container_set_obj: invalid obj");
        return 0;
    }
    auto obj = gLuaContainerHolder.Get(obj_pointer);
    if (!obj) {
        luaL_error(L, "cpp_table_container_set_obj: no obj found %p", obj_pointer);
        return 0;
    }
    // check message_id avoid set wrong obj
    if (obj->GetMessageId() != message_id) {
        luaL_error(L, "cpp_table_container_set_obj: %s invalid message_id %d", obj->GetName().data(), message_id);
        return 0;
    }
    auto ret = container->SetSharedObj<Container>(idx, obj, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_obj: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    return 0;
}

static int cpp_table_container_get_array(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_array: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_array: no container found %p", pointer);
        return 0;
    }
    ArrayPtr array;
    bool is_nil = false;
    auto ret = container->GetSharedObj<Array>(idx, array, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_array: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    auto array_pointer = array.get();
    auto find = cpp_table_get_array_container_userdata(L, array_pointer);
    if (!find) {
        auto userdata_pointer = (void **) lua_newuserdata(L, sizeof(void *));
        *userdata_pointer = array.get();
        gLuaContainerHolder.SetArray(userdata_pointer, array);
        cpp_table_reg_array_container_userdata(L, array_pointer, array->GetLayoutMember()->key);
        LLOG("cpp_table_container_get_array: %s new %p", array->GetName().data(), array_pointer);
    } else {
        LLOG("cpp_table_container_get_array: %s already exist %p", array->GetName().data(), array_pointer);
    }
    return 1;
}

static int cpp_table_container_set_array(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TUSERDATA && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_array: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    void *array_pointer = lua_touserdata(L, 3);
    bool is_nil = lua_isnil(L, 3);
    int message_id = lua_tointeger(L, 4);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_array: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_array: no container found %p", pointer);
        return 0;
    }
    if (!array_pointer) {
        luaL_error(L, "cpp_table_container_set_array: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(array_pointer);
    if (!array) {
        luaL_error(L, "cpp_table_container_set_array: no array found %p", array_pointer);
        return 0;
    }
    // check message_id avoid set wrong obj
    if (array->GetMessageId() != message_id) {
        luaL_error(L, "cpp_table_container_set_array: %s invalid message_id %d", array->GetName().data(), message_id);
        return 0;
    }
    auto ret = container->SetSharedObj<Array>(idx, array, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_array: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    return 0;
}

static int cpp_table_container_get_map(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_map: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_map: no container found %p", pointer);
        return 0;
    }
    MapPtr map;
    bool is_nil = false;
    auto ret = container->GetSharedObj<Map>(idx, map, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_map: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    auto map_pointer = map.get();
    auto find = cpp_table_get_map_container_userdata(L, map_pointer);
    if (!find) {
        auto userdata_pointer = (void **) lua_newuserdata(L, sizeof(void *));
        *userdata_pointer = map.get();
        gLuaContainerHolder.SetMap(userdata_pointer, map);
        cpp_table_reg_map_container_userdata(L, map_pointer, map->GetLayoutMember()->key,
                                             map->GetLayoutMember()->value);
        LLOG("cpp_table_container_get_map: %s new %p", map->GetName().data(), map_pointer);
    } else {
        LLOG("cpp_table_container_get_map: %s already exist %p", map->GetName().data(), map_pointer);
    }
    return 1;
}

static int cpp_table_container_set_map(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TUSERDATA && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_map: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    void *map_pointer = lua_touserdata(L, 3);
    bool is_nil = lua_isnil(L, 3);
    int key_message_id = lua_tointeger(L, 4);
    int value_message_id = lua_tointeger(L, 5);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_map: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_map: no container found %p", pointer);
        return 0;
    }
    if (!map_pointer) {
        luaL_error(L, "cpp_table_container_set_map: invalid array");
        return 0;
    }
    auto map = gLuaContainerHolder.GetMap(map_pointer);
    if (!map) {
        luaL_error(L, "cpp_table_container_set_map: no array found %p", map_pointer);
        return 0;
    }
    // check message_id avoid set wrong obj
    if (map->GetKeyMessageId() != key_message_id) {
        luaL_error(L, "cpp_table_container_set_map: %s invalid message_id %d", map->GetName().data(), key_message_id);
        return 0;
    }
    if (map->GetValueMessageId() != value_message_id) {
        luaL_error(L, "cpp_table_container_set_map: %s invalid message_id %d", map->GetName().data(), value_message_id);
        return 0;
    }
    auto ret = container->SetSharedObj<Map>(idx, map, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_map: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    return 0;
}

static int cpp_table_create_array_container(lua_State *L) {
    size_t name_size = 0;
    const char *name = lua_tolstring(L, 1, &name_size);
    if (name_size == 0) {
        luaL_error(L, "cpp_table_create_array_container: invalid name %s", name);
        return 0;
    }
    int tag = lua_tointeger(L, 2);
    if (tag < 0) {
        luaL_error(L, "cpp_table_create_array_container: invalid tag %d", tag);
        return 0;
    }
    auto layout_key = gStringHeap.Add(HashStringView(name, name_size));
    auto layout = gLayoutMgr.GetLayout(layout_key);
    if (!layout) {
        luaL_error(L, "cpp_table_create_array_container: no layout found %s", name);
        return 0;
    }
    auto layout_member = layout->GetMember(tag);
    if (!layout_member) {
        luaL_error(L, "cpp_table_create_array_container: no layout member found %s %d", name, tag);
        return 0;
    }
    auto array = MakeShared<Array>(layout_member);
    auto pointer = (void **) lua_newuserdata(L, sizeof(void *));
    *pointer = array.get();
    gLuaContainerHolder.SetArray(pointer, array);
    // create array pointer -> userdata pointer mapping in lua _G.CPP_TABLE_ARRAY_CONTAINER which is a weak table
    cpp_table_reg_array_container_userdata(L, array.get(), layout_member->key);
    return 1;
}

static int cpp_table_delete_array_container(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    if (!pointer) {
        luaL_error(L, "cpp_table_delete_array_container: invalid pointer");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_delete_array_container: no array found %p", pointer);
        return 0;
    }
    cpp_table_remove_array_container_userdata(L, array.get());
    gLuaContainerHolder.RemoveArray(pointer);
    return 0;
}

static int cpp_table_array_container_get_int32(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_get_int32: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_get_int32: no array found %p", pointer);
        return 0;
    }
    int32_t value = 0;
    bool is_nil = false;
    auto ret = array->Get<int32_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_get_int32: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, value);
    return 1;
}

static int cpp_table_array_container_set_int32(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_array_container_set_int32: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    int32_t value = lua_tointeger(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_set_int32: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_set_int32: no array found %p", pointer);
        return 0;
    }
    auto ret = array->Set<int32_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_set_int32: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_array_container_get_uint32(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_get_uint32: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_get_uint32: no array found %p", pointer);
        return 0;
    }
    uint32_t value = 0;
    bool is_nil = false;
    auto ret = array->Get<uint32_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_get_uint32: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, value);
    return 1;
}

static int cpp_table_array_container_set_uint32(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_array_container_set_uint32: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    uint32_t value = lua_tointeger(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_set_uint32: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_set_uint32: no array found %p", pointer);
        return 0;
    }
    auto ret = array->Set<uint32_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_set_uint32: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_array_container_get_int64(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_get_int64: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_get_int64: no array found %p", pointer);
        return 0;
    }
    int64_t value = 0;
    bool is_nil = false;
    auto ret = array->Get<int64_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_get_int64: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, value);
    return 1;
}

static int cpp_table_array_container_set_int64(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_array_container_set_int64: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    int64_t value = lua_tointeger(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_set_int64: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_set_int64: no array found %p", pointer);
        return 0;
    }
    auto ret = array->Set<int64_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_set_int64: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_array_container_get_uint64(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_get_uint64: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_get_uint64: no array found %p", pointer);
        return 0;
    }
    uint64_t value = 0;
    bool is_nil = false;
    auto ret = array->Get<uint64_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_get_uint64: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, value);
    return 1;
}

static int cpp_table_array_container_set_uint64(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_array_container_set_uint64: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    uint64_t value = lua_tointeger(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_set_uint64: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_set_uint64: no array found %p", pointer);
        return 0;
    }
    auto ret = array->Set<uint64_t>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_set_uint64: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_array_container_get_float(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_get_float: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_get_float: no array found %p", pointer);
        return 0;
    }
    float value = 0;
    bool is_nil = false;
    auto ret = array->Get<float>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_get_float: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushnumber(L, value);
    return 1;
}

static int cpp_table_array_container_set_float(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_array_container_set_float: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    float value = lua_tonumber(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_set_float: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_set_float: no array found %p", pointer);
        return 0;
    }
    auto ret = array->Set<float>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_set_float: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_array_container_get_double(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_get_double: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_get_double: no array found %p", pointer);
        return 0;
    }
    double value = 0;
    bool is_nil = false;
    auto ret = array->Get<double>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_get_double: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushnumber(L, value);
    return 1;
}

static int cpp_table_array_container_set_double(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_array_container_set_double: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    double value = lua_tonumber(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_set_double: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_set_double: no array found %p", pointer);
        return 0;
    }
    auto ret = array->Set<double>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_set_double: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_array_container_get_bool(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_get_bool: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_get_bool: no array found %p", pointer);
        return 0;
    }
    bool value = false;
    bool is_nil = false;
    auto ret = array->Get<bool>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_get_bool: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushboolean(L, value);
    return 1;
}

static int cpp_table_array_container_set_bool(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TBOOLEAN && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_array_container_set_bool: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    bool value = lua_toboolean(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_set_bool: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_set_bool: no array found %p", pointer);
        return 0;
    }
    auto ret = array->Set<bool>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_set_bool: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    lua_pushboolean(L, 1);
    return 0;
}

static int cpp_table_array_container_get_string(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_get_string: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_get_string: no array found %p", pointer);
        return 0;
    }
    StringPtr value;
    bool is_nil = false;
    auto ret = array->GetSharedObj<String>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_get_string: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, value->c_str(), value->size());
    return 1;
}

static int cpp_table_array_container_set_string(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    size_t size = 0;
    if (lua_type(L, 3) != LUA_TSTRING && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_array_container_set_string: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    const char *str = lua_tolstring(L, 3, &size);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_set_string: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_set_string: no array found %p", pointer);
        return 0;
    }
    StringPtr shared_str;
    if (!is_nil) {
        shared_str = gStringHeap.Add(HashStringView(str, size));
    }
    auto ret = array->SetSharedObj(idx, shared_str, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_set_string: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    return 0;
}

static int cpp_table_array_container_get_obj(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_get_obj: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_get_obj: no array found %p", pointer);
        return 0;
    }
    ContainerPtr obj;
    bool is_nil = false;
    auto ret = array->GetSharedObj<Container>(idx, obj, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_get_obj: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
    auto container_pointer = obj.get();
    cpp_table_get_container_push_pointer(L, container_pointer);
    return 1;
}

static int cpp_table_array_container_set_obj(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (lua_type(L, 3) != LUA_TUSERDATA && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_array_container_set_obj: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    void *obj_pointer = lua_touserdata(L, 3);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_get_obj: invalid array");
        return 0;
    }
    int message_id = lua_tointeger(L, 4);
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_get_obj: no array found %p", pointer);
        return 0;
    }
    if (!obj_pointer) {
        luaL_error(L, "cpp_table_array_container_set_obj: invalid obj");
        return 0;
    }
    auto obj = gLuaContainerHolder.Get(obj_pointer);
    if (!obj) {
        luaL_error(L, "cpp_table_array_container_set_obj: no obj found %p", obj_pointer);
        return 0;
    }
    // check message_id avoid set wrong obj
    if (obj->GetMessageId() != message_id) {
        luaL_error(L, "cpp_table_array_container_set_obj: %s invalid message_id %d", obj->GetName().data(), message_id);
        return 0;
    }
    auto ret = array->SetSharedObj<Container>(idx, obj, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_set_obj: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    return 0;
}

static int cpp_table_create_map_container(lua_State *L) {
    size_t name_size = 0;
    const char *name = lua_tolstring(L, 1, &name_size);
    if (name_size == 0) {
        luaL_error(L, "cpp_table_create_array_container: invalid name %s", name);
        return 0;
    }
    int tag = lua_tointeger(L, 2);
    if (tag < 0) {
        luaL_error(L, "cpp_table_create_array_container: invalid tag %d", tag);
        return 0;
    }
    auto layout_key = gStringHeap.Add(HashStringView(name, name_size));
    auto layout = gLayoutMgr.GetLayout(layout_key);
    if (!layout) {
        luaL_error(L, "cpp_table_create_array_container: no layout found %s", name);
        return 0;
    }
    auto layout_member = layout->GetMember(tag);
    if (!layout_member) {
        luaL_error(L, "cpp_table_create_array_container: no layout member found %s %d", name, tag);
        return 0;
    }
    auto map = MakeShared<Map>(layout_member);
    auto pointer = (void **) lua_newuserdata(L, sizeof(void *));
    *pointer = map.get();
    gLuaContainerHolder.SetMap(pointer, map);
    // create map pointer -> userdata pointer mapping in lua _G.CPP_TABLE_MAP_CONTAINER which is a weak table
    cpp_table_reg_map_container_userdata(L, map.get(), layout_member->key, layout_member->value);
    return 1;
}

static int cpp_table_map_container_get_by_int32(lua_State *L, MapPtr map, int32_t key, int value_message_id) {
    bool is_nil = false;
    switch (value_message_id) {
        case mt_int32: {
            auto ret = map->Get32by32(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushinteger(L, ret.m_32);
            return 1;
        }
        case mt_uint32: {
            auto ret = map->Get32by32(key, is_nil);
            if (is_nil) {
                return 1;
            }
            lua_pushinteger(L, ret.m_u32);
            return 1;
        }
        case mt_int64: {
            auto ret = map->Get64by32(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushinteger(L, ret.m_64);
            return 1;
        }
        case mt_uint64: {
            auto ret = map->Get64by32(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushinteger(L, ret.m_u64);
            return 1;
        }
        case mt_float: {
            auto ret = map->Get32by32(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushnumber(L, ret.m_float);
            return 1;
        }
        case mt_double: {
            auto ret = map->Get64by32(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushnumber(L, ret.m_double);
            return 1;
        }
        case mt_bool: {
            auto ret = map->Get32by32(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushboolean(L, ret.m_bool);
            return 1;
        }
        case mt_string: {
            auto ret = map->Get64by32(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushlstring(L, ret.m_string->c_str(), ret.m_string->size());
            return 1;
        }
        default: {
            auto ret = map->Get64by32(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            auto obj = ret.m_obj;
            cpp_table_get_container_push_pointer(L, obj);
            return 1;
        }
    }
}

static int cpp_table_map_container_get_by_int64(lua_State *L, MapPtr map, int64_t key, int value_message_id) {
    bool is_nil = false;
    switch (value_message_id) {
        case mt_int32: {
            auto ret = map->Get32by64(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushinteger(L, ret.m_32);
            return 1;
        }
        case mt_uint32: {
            auto ret = map->Get32by64(key, is_nil);
            if (is_nil) {
                return 1;
            }
            lua_pushinteger(L, ret.m_u32);
            return 1;
        }
        case mt_int64: {
            auto ret = map->Get64by64(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushinteger(L, ret.m_64);
            return 1;
        }
        case mt_uint64: {
            auto ret = map->Get64by64(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushinteger(L, ret.m_u64);
            return 1;
        }
        case mt_float: {
            auto ret = map->Get32by64(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushnumber(L, ret.m_float);
            return 1;
        }
        case mt_double: {
            auto ret = map->Get64by64(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushnumber(L, ret.m_double);
            return 1;
        }
        case mt_bool: {
            auto ret = map->Get32by64(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushboolean(L, ret.m_bool);
            return 1;
        }
        case mt_string: {
            auto ret = map->Get64by64(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushlstring(L, ret.m_string->c_str(), ret.m_string->size());
            return 1;
        }
        default: {
            auto ret = map->Get64by64(key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            auto obj = ret.m_obj;
            cpp_table_get_container_push_pointer(L, obj);
            return 1;
        }
    }
}

static int cpp_table_map_container_get(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int key_message_id = lua_tointeger(L, 3);
    int value_message_id = lua_tointeger(L, 4);
    if (!pointer) {
        luaL_error(L, "cpp_table_map_container_get: invalid map");
        return 0;
    }
    auto map = gLuaContainerHolder.GetMap(pointer);
    if (!map) {
        luaL_error(L, "cpp_table_map_container_get: no map found %p", pointer);
        return 0;
    }

    // no data, just return nil
    auto m = map->GetMap();
    if (!m.m_void) {
        lua_pushnil(L);
        return 1;
    }

    switch (key_message_id) {
        case mt_int32: {
            int32_t key = (int32_t) lua_tointeger(L, 2);
            return cpp_table_map_container_get_by_int32(L, map, key, value_message_id);
        }
        case mt_uint32: {
            uint32_t key = (uint32_t) lua_tointeger(L, 2);
            return cpp_table_map_container_get_by_int32(L, map, (int32_t) key, value_message_id);
        }
        case mt_int64: {
            int64_t key = (int64_t) lua_tointeger(L, 2);
            return cpp_table_map_container_get_by_int64(L, map, key, value_message_id);
        }
        case mt_uint64: {
            uint64_t key = (uint64_t) lua_tointeger(L, 2);
            return cpp_table_map_container_get_by_int64(L, map, (int64_t) key, value_message_id);
        }
        case mt_float: {
            luaL_error(L, "cpp_table_map_container_get: invalid key type %d", key_message_id);
            return 0;
        }
        case mt_double: {
            luaL_error(L, "cpp_table_map_container_get: invalid key type %d", key_message_id);
            return 0;
        }
        case mt_bool: {
            bool key = (bool) lua_toboolean(L, 2);
            return cpp_table_map_container_get_by_int32(L, map, (int32_t) key, value_message_id);
        }
        case mt_string: {
            size_t size = 0;
            const char *str = lua_tolstring(L, 2, &size);
            auto shared_str = gStringHeap.Add(HashStringView(str, size));
            return cpp_table_map_container_get_by_int64(L, map, (int64_t) shared_str.get(), value_message_id);
        }
        default: {
            luaL_error(L, "cpp_table_map_container_get: invalid key type %d", key_message_id);
            return 0;
        }
    }
}

static int cpp_table_map_container_set(lua_State *L) {
    return 0;
}

static int cpp_table_delete_map_container(lua_State *L) {
    return 0;
}

}

std::vector<luaL_Reg> GetCppTableFuncs() {
    return {
            {"cpp_table_set_message_id",             cpp_table::cpp_table_set_message_id},
            {"cpp_table_update_layout",              cpp_table::cpp_table_update_layout},

            {"cpp_table_create_container",           cpp_table::cpp_table_create_container},
            {"cpp_table_delete_container",           cpp_table::cpp_table_delete_container},
            {"cpp_table_container_get_int32",        cpp_table::cpp_table_container_get_int32},
            {"cpp_table_container_set_int32",        cpp_table::cpp_table_container_set_int32},
            {"cpp_table_container_get_uint32",       cpp_table::cpp_table_container_get_uint32},
            {"cpp_table_container_set_uint32",       cpp_table::cpp_table_container_set_uint32},
            {"cpp_table_container_get_int64",        cpp_table::cpp_table_container_get_int64},
            {"cpp_table_container_set_int64",        cpp_table::cpp_table_container_set_int64},
            {"cpp_table_container_get_uint64",       cpp_table::cpp_table_container_get_uint64},
            {"cpp_table_container_set_uint64",       cpp_table::cpp_table_container_set_uint64},
            {"cpp_table_container_get_float",        cpp_table::cpp_table_container_get_float},
            {"cpp_table_container_set_float",        cpp_table::cpp_table_container_set_float},
            {"cpp_table_container_get_double",       cpp_table::cpp_table_container_get_double},
            {"cpp_table_container_set_double",       cpp_table::cpp_table_container_set_double},
            {"cpp_table_container_get_bool",         cpp_table::cpp_table_container_get_bool},
            {"cpp_table_container_set_bool",         cpp_table::cpp_table_container_set_bool},
            {"cpp_table_container_get_string",       cpp_table::cpp_table_container_get_string},
            {"cpp_table_container_set_string",       cpp_table::cpp_table_container_set_string},
            {"cpp_table_container_get_obj",          cpp_table::cpp_table_container_get_obj},
            {"cpp_table_container_set_obj",          cpp_table::cpp_table_container_set_obj},
            {"cpp_table_container_get_array",        cpp_table::cpp_table_container_get_array},
            {"cpp_table_container_set_array",        cpp_table::cpp_table_container_set_array},
            {"cpp_table_container_get_map",          cpp_table::cpp_table_container_get_map},
            {"cpp_table_container_set_map",          cpp_table::cpp_table_container_set_map},

            {"cpp_table_create_array_container",     cpp_table::cpp_table_create_array_container},
            {"cpp_table_delete_array_container",     cpp_table::cpp_table_delete_array_container},
            {"cpp_table_array_container_get_int32",  cpp_table::cpp_table_array_container_get_int32},
            {"cpp_table_array_container_set_int32",  cpp_table::cpp_table_array_container_set_int32},
            {"cpp_table_array_container_get_uint32", cpp_table::cpp_table_array_container_get_uint32},
            {"cpp_table_array_container_set_uint32", cpp_table::cpp_table_array_container_set_uint32},
            {"cpp_table_array_container_get_int64",  cpp_table::cpp_table_array_container_get_int64},
            {"cpp_table_array_container_set_int64",  cpp_table::cpp_table_array_container_set_int64},
            {"cpp_table_array_container_get_uint64", cpp_table::cpp_table_array_container_get_uint64},
            {"cpp_table_array_container_set_uint64", cpp_table::cpp_table_array_container_set_uint64},
            {"cpp_table_array_container_get_float",  cpp_table::cpp_table_array_container_get_float},
            {"cpp_table_array_container_set_float",  cpp_table::cpp_table_array_container_set_float},
            {"cpp_table_array_container_get_double", cpp_table::cpp_table_array_container_get_double},
            {"cpp_table_array_container_set_double", cpp_table::cpp_table_array_container_set_double},
            {"cpp_table_array_container_get_bool",   cpp_table::cpp_table_array_container_get_bool},
            {"cpp_table_array_container_set_bool",   cpp_table::cpp_table_array_container_set_bool},
            {"cpp_table_array_container_get_string", cpp_table::cpp_table_array_container_get_string},
            {"cpp_table_array_container_set_string", cpp_table::cpp_table_array_container_set_string},
            {"cpp_table_array_container_get_obj",    cpp_table::cpp_table_array_container_get_obj},
            {"cpp_table_array_container_set_obj",    cpp_table::cpp_table_array_container_set_obj},

            {"cpp_table_create_map_container",       cpp_table::cpp_table_create_map_container},
            {"cpp_table_map_container_get",          cpp_table::cpp_table_map_container_get},
            {"cpp_table_map_container_set",          cpp_table::cpp_table_map_container_set},
            {"core_cpp_table_delete_map_container",  cpp_table::cpp_table_delete_map_container},
    };
}
