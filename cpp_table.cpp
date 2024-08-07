#include "cpp_table.h"

namespace cpp_table {

StringHeap gStringHeap;
LuaContainerHolder gLuaContainerHolder;
LayoutMgr gLayoutMgr;

void RefCntObj::Delete() {
    switch (m_type) {
        case rot_container: {
            ((Container *) this)->~Container();
            break;
        }
        case rot_array: {
            ((Array *) this)->~Array();
            break;
        }
        case rot_map: {
            ((Map *) this)->~Map();
            break;
        }
        case rot_string: {
            ((String *) this)->~String();
            break;
        }

        case rot_layout : {
            ((Layout *) this)->~Layout();
            break;
        }
        case rot_layout_member: {
            ((Layout::Member *) this)->~Member();
            break;
        }
        default: {
            LERR("RefCntObj::Delete: invalid type %d", m_type);
            break;
        }
    }
    free(this);
}

String::~String() {
    if (m_len) {
        gStringHeap.Remove(StringView(m_str, m_len));
    }
}

Container::Container(LayoutPtr layout) : RefCntObj(rot_container) {
    m_layout = layout;
    LLOG("Container::Container: %s %p", GetName().data(), this);
}

Container::~Container() {
    LLOG("Container::~Container: %s %p", GetName().data(), this);
    ReleaseAllSharedObj();
    if (m_buffer) {
        delete[] m_buffer;
    }
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

Array::Array(Layout::MemberPtr layout_member) : RefCntObj(rot_array) {
    m_layout_member = layout_member;
}

Array::~Array() {
    LLOG("Array::~Array: %s %p", GetName().data(), this);
    ReleaseAllSharedObj();
    if (m_buffer) {
        delete[] m_buffer;
    }
}

void Array::ReleaseAllSharedObj() {
    if (!m_layout_member->key_shared) {
        return;
    }
    int element_size = m_buffer_size / m_layout_member->key_size;
    for (int i = 0; i < element_size; ++i) {
        SharedPtr<RefCntObj> out;
        bool is_nil = false;
        auto ret = GetSharedObj<RefCntObj>(i, out, is_nil);
        if (!ret) {
            LERR("Array::ReleaseAllSh1aredObj: %s invalid pos %d", m_layout_member->name->data(), i);
            return;
        }
        if (!is_nil) {
            out->Release();
        }
    }
}

Map::Map(Layout::MemberPtr layout_member) : RefCntObj(rot_map) {
    m_layout_member = layout_member;
    m_map.m_void = 0;
    LLOG("Map::Map: %s %p", layout_member->name->data(), this);
}

Map::~Map() {
    LLOG("Map::~Map: %s %p", GetName().data(), this);
    ReleaseAllSharedObj();
}

void Map::ReleaseAllSharedObj() {
    if (!m_map.m_void) {
        return;
    }

    int key_message_id = m_layout_member->message_id;
    int value_message_id = m_layout_member->value_message_id;

    switch (key_message_id) {
        case mt_int32:
        case mt_uint32:
        case mt_bool: {
            switch (value_message_id) {
                case mt_int32:
                case mt_uint32:
                case mt_float:
                case mt_bool: {
                    delete m_map.m_32_32;
                    m_map.m_32_32 = 0;
                    break;
                }
                case mt_int64:
                case mt_uint64:
                case mt_double: {
                    delete m_map.m_32_64;
                    m_map.m_32_64 = 0;
                    break;
                }
                case mt_string: {
                    ReleaseStrBy32();
                    delete m_map.m_32_64;
                    m_map.m_32_64 = 0;
                    break;
                }
                default: {
                    ReleaseObjBy32();
                    delete m_map.m_32_64;
                    m_map.m_32_64 = 0;
                    break;
                }
            }
            break;
        }

        case mt_int64:
        case mt_uint64: {
            switch (value_message_id) {
                case mt_int32:
                case mt_uint32:
                case mt_float:
                case mt_bool: {
                    delete m_map.m_64_32;
                    m_map.m_64_32 = 0;
                    break;
                }
                case mt_int64:
                case mt_uint64:
                case mt_double: {
                    delete m_map.m_64_64;
                    m_map.m_64_64 = 0;
                    break;
                }
                case mt_string: {
                    ReleaseStrBy64();
                    delete m_map.m_64_64;
                    m_map.m_64_64 = 0;
                    break;
                }
                default: {
                    ReleaseObjBy64();
                    delete m_map.m_64_64;
                    m_map.m_64_64 = 0;
                    break;
                }
            }
            break;
        }

        case mt_string: {
            switch (value_message_id) {
                case mt_int32:
                case mt_uint32:
                case mt_float:
                case mt_bool: {
                    delete m_map.m_string_32;
                    m_map.m_string_32 = 0;
                    break;
                }
                case mt_int64:
                case mt_uint64:
                case mt_double: {
                    delete m_map.m_string_64;
                    m_map.m_string_64 = 0;
                    break;
                }
                case mt_string: {
                    ReleaseStrByString();
                    delete m_map.m_string_64;
                    m_map.m_string_64 = 0;
                    break;
                }
                default: {
                    ReleaseObjByString();
                    delete m_map.m_string_64;
                    m_map.m_string_64 = 0;
                    break;
                }
            }
            break;
        }

        default: {
            LERR("Map::ReleaseAllSharedObj: %s invalid key message_id %d", m_layout_member->name->data(),
                 key_message_id);
            return;
        }
    }
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
    auto layout_key = gStringHeap.Add(StringView(name, name_size));
    gLayoutMgr.SetMessageId(layout_key, message_id);
    return 0;
}

static bool cpp_table_get_layout_member_number(lua_State *L, const char *name, int &ret) {
    lua_pushstring(L, name);
    lua_gettable(L, -2);
    if (lua_type(L, -1) != LUA_TNUMBER) {
        luaL_error(L, "cpp_table_get_layout_member_number: invalid type type %s %d", name, lua_type(L, -1));
        return false;
    }
    ret = lua_tointeger(L, -1);
    lua_pop(L, 1);
    if (ret < 0) {
        luaL_error(L, "cpp_table_get_layout_member_number: invalid value %s %d", name, ret);
        return false;
    }
    return true;
}

static bool cpp_table_get_layout_member_string(lua_State *L, const char *name, StringPtr &ret, bool can_empty = false) {
    lua_pushstring(L, name);
    lua_gettable(L, -2);
    if (lua_type(L, -1) != LUA_TSTRING) {
        luaL_error(L, "cpp_table_get_layout_member_string: invalid type type %s %d", name, lua_type(L, -1));
        return false;
    }
    size_t str_size = 0;
    const char *str = lua_tolstring(L, -1, &str_size);
    if (str == 0) {
        if (!can_empty) {
            luaL_error(L, "cpp_table_get_layout_member_string: invalid type %s", name);
            return false;
        } else {
            str = "";
            str_size = 0;
        }
    }
    ret = gStringHeap.Add(StringView(str, str_size));
    lua_pop(L, 1);
    return true;
}

static int cpp_table_update_layout(lua_State *L) {
    size_t name_size = 0;
    const char *name = lua_tolstring(L, 1, &name_size);
    if (name_size == 0) {
        luaL_error(L, "cpp_table_update_layout: invalid name %s", name);
        return 0;
    }
    luaL_checktype(L, 2, LUA_TTABLE);

    auto layout_key = gStringHeap.Add(StringView(name, name_size));
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
        mem->name = gStringHeap.Add(StringView(name, name_size));

        if (lua_type(L, -1) != LUA_TTABLE) {
            luaL_error(L, "cpp_table_update_layout: invalid table type %d", lua_type(L, -1));
            return 0;
        }

        // type
        if (!cpp_table_get_layout_member_string(L, "type", mem->type)) {
            return 0;
        }

        // key
        if (!cpp_table_get_layout_member_string(L, "key", mem->key)) {
            return 0;
        }

        // value
        if (!cpp_table_get_layout_member_string(L, "value", mem->value, true)) {
            return 0;
        }

        // pos
        if (!cpp_table_get_layout_member_number(L, "pos", mem->pos)) {
            return 0;
        }

        // size
        if (!cpp_table_get_layout_member_number(L, "size", mem->size)) {
            return 0;
        }

        // tag
        if (!cpp_table_get_layout_member_number(L, "tag", mem->tag)) {
            return 0;
        }

        // shared
        if (!cpp_table_get_layout_member_number(L, "shared", mem->shared)) {
            return 0;
        }

        // message_id
        if (!cpp_table_get_layout_member_number(L, "message_id", mem->message_id)) {
            return 0;
        }

        // value_message_id
        if (!cpp_table_get_layout_member_number(L, "value_message_id", mem->value_message_id)) {
            return 0;
        }

        // key_size
        if (!cpp_table_get_layout_member_number(L, "key_size", mem->key_size)) {
            return 0;
        }

        // key_shared
        if (!cpp_table_get_layout_member_number(L, "key_shared", mem->key_shared)) {
            return 0;
        }

        auto old = layout->GetMember(mem->tag);
        if (old) {
            old->CopyFrom(mem.get());
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

static int cpp_table_dump_statistic(lua_State *L) {
    lua_newtable(L);

    auto ret = gStringHeap.Dump();
    lua_pushstring(L, "string_heap");
    lua_newtable(L);
    for (size_t i = 0; i < ret.size(); ++i) {
        lua_pushinteger(L, i + 1);
        lua_pushlstring(L, ret[i]->data(), ret[i]->size());
        lua_settable(L, -3);
    }
    lua_settable(L, -3);

    lua_pushstring(L, "container_size");
    lua_pushinteger(L, gLuaContainerHolder.GetContainerSize());
    lua_settable(L, -3);

    lua_pushstring(L, "array_size");
    lua_pushinteger(L, gLuaContainerHolder.GetArraySize());
    lua_settable(L, -3);

    lua_pushstring(L, "map_size");
    lua_pushinteger(L, gLuaContainerHolder.GetMapSize());
    lua_settable(L, -3);
    return 1;
}

static void cpp_table_reg_userdata(lua_State *L, void *p, const std::string &lua_table_name,
                                   const std::string &lua_layout_table_name, const std::string &lua_key_name) {
    // set meta table from lua_layout_table_name
    lua_getglobal(L, lua_layout_table_name.c_str());// stack: 1: userdata, 2: global meta
    if (lua_type(L, -1) != LUA_TTABLE) { // stack: 1: userdata, 2: nil
        lua_pop(L, 1); // stack: 1: userdata
        luaL_error(L, "cpp_table_reg_userdata: no %s found", lua_layout_table_name.c_str());
        return;
    }
    lua_pushlstring(L, lua_key_name.data(), lua_key_name.size()); // stack: 1: userdata, 2: global meta, 3: name
    lua_gettable(L, -2); // stack: 1: userdata, 2: global meta, 3: meta
    if (lua_type(L, -1) != LUA_TTABLE) {
        lua_pop(L, 1); // stack: 1: userdata, 2: global meta
        luaL_error(L, "cpp_table_reg_userdata: no meta table found %s", lua_key_name.data());
        return;
    }
    lua_setmetatable(L, -3); // stack: 1: userdata, 2: global meta
    lua_pop(L, 1); // stack: 1: userdata
}

static void cpp_table_reg_container_userdata(lua_State *L, Container *container) {
    cpp_table_reg_userdata(L, container, "CPP_TABLE_CONTAINER", "CPP_TABLE_LAYOUT_META_TABLE",
                           std::string(container->GetName().data(), container->GetName().size()));
}

static void cpp_table_get_container_push_pointer(lua_State *L, Container *container_pointer) {
    auto userdata_pointer = lua_newuserdata(L, 0);
    gLuaContainerHolder.Set(userdata_pointer, container_pointer);
    cpp_table_reg_container_userdata(L, container_pointer);
    LLOG("cpp_table_get_container_push_pointer: %s new %p", container_pointer->GetName().data(), container_pointer);
}

static void cpp_table_reg_array_container_userdata(lua_State *L, Array *array, StringPtr key) {
    cpp_table_reg_userdata(L, array, "CPP_TABLE_ARRAY_CONTAINER", "CPP_TABLE_LAYOUT_ARRAY_META_TABLE",
                           std::string(key->data(), key->size()));
}

static void
cpp_table_reg_map_container_userdata(lua_State *L, Map *map, StringPtr key, StringPtr value) {
    std::string key_value = std::string(key->data(), key->size()) + "-" + std::string(value->data(), value->size());
    cpp_table_reg_userdata(L, map, "CPP_TABLE_MAP_CONTAINER", "CPP_TABLE_LAYOUT_MAP_META_TABLE", key_value);
}

static int cpp_table_create_container(lua_State *L) {
    size_t name_size = 0;
    const char *name = lua_tolstring(L, 1, &name_size);
    if (name_size == 0) {
        luaL_error(L, "cpp_table_create_container: invalid name %s", name);
        return 0;
    }
    auto layout_key = gStringHeap.Add(StringView(name, name_size));
    auto layout = gLayoutMgr.GetLayout(layout_key);
    if (!layout) {
        luaL_error(L, "cpp_table_create_container: no layout found %s", name);
        return 0;
    }
    auto container = MakeShared<Container>(layout);
    auto pointer = lua_newuserdata(L, 0);
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
    LLOG("cpp_table_delete_container: %s %p", container->GetName().data(), container.get());
    gLuaContainerHolder.Remove(pointer);
    return 0;
}

template<typename T, typename V>
void lua_push_helper(lua_State *L, V v, T value) {
    static_assert(true, "lua_push_helper: invalid type");
}

template<typename V>
void lua_push_helper(lua_State *L, V v, int32_t value) {
    lua_pushinteger(L, value);
}

template<typename V>
void lua_push_helper(lua_State *L, V v, uint32_t value) {
    lua_pushinteger(L, value);
}

template<typename V>
void lua_push_helper(lua_State *L, V v, int64_t value) {
    lua_pushinteger(L, value);
}

template<typename V>
void lua_push_helper(lua_State *L, V v, uint64_t value) {
    lua_pushinteger(L, value);
}

template<typename V>
void lua_push_helper(lua_State *L, V v, bool value) {
    lua_pushboolean(L, value);
}

template<typename V>
void lua_push_helper(lua_State *L, V v, float value) {
    lua_pushnumber(L, value);
}

template<typename V>
void lua_push_helper(lua_State *L, V v, double value) {
    lua_pushnumber(L, value);
}

template<typename T>
int cpp_table_container_get_normal(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_get_normal: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_get_normal: no container found %p", pointer);
        return 0;
    }
    T value = 0;
    bool is_nil = false;
    auto ret = container->Get<T>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_container_get_normal: %s invalid idx %d", container->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    if constexpr (std::is_same<T, int32_t>::value || std::is_same<T, uint32_t>::value ||
                  std::is_same<T, int64_t>::value || std::is_same<T, uint64_t>::value) {
        lua_pushinteger(L, value);
    } else if constexpr (std::is_same<T, bool>::value) {
        lua_pushboolean(L, value);
    } else if constexpr (std::is_same<T, float>::value || std::is_same<T, double>::value) {
        lua_pushnumber(L, value);
    } else {
        luaL_error(L, "cpp_table_container_get_normal: invalid type %s %s", container->GetName().data(),
                   typeid(T).name());
        return 0;
    }
#else
    lua_push_helper(L, container, value);
#endif
    return 1;
}


template<typename T>
int lua_get_type(lua_State *L) {
    static_assert(true, "lua_get_type: invalid type");
    return LUA_TNIL;
}

template<>
int lua_get_type<int32_t>(lua_State *L) {
    return LUA_TNUMBER;
}

template<>
int lua_get_type<uint32_t>(lua_State *L) {
    return LUA_TNUMBER;
}

template<>
int lua_get_type<int64_t>(lua_State *L) {
    return LUA_TNUMBER;
}

template<>
int lua_get_type<uint64_t>(lua_State *L) {
    return LUA_TNUMBER;
}

template<>
int lua_get_type<bool>(lua_State *L) {
    return LUA_TBOOLEAN;
}

template<>
int lua_get_type<float>(lua_State *L) {
    return LUA_TNUMBER;
}

template<>
int lua_get_type<double>(lua_State *L) {
    return LUA_TNUMBER;
}

template<typename T, typename V>
T lua_get_helper(lua_State *L, V v, int pos) {
    static_assert(true, "lua_get_helper: invalid type");
    return T();
}

template<typename V>
int32_t lua_get_helper(lua_State *L, V v, int pos) {
    return lua_tointeger(L, pos);
}

template<typename V>
uint32_t lua_get_helper(lua_State *L, V v, int pos) {
    return lua_tointeger(L, pos);
}

template<typename V>
int64_t lua_get_helper(lua_State *L, V v, int pos) {
    return lua_tointeger(L, pos);
}

template<typename V>
uint64_t lua_get_helper(lua_State *L, V v, int pos) {
    return lua_tointeger(L, pos);
}

template<typename V>
bool lua_get_helper(lua_State *L, V v, int pos) {
    return lua_toboolean(L, pos);
}

template<typename V>
float lua_get_helper(lua_State *L, V v, int pos) {
    return lua_tonumber(L, pos);
}

template<typename V>
double lua_get_helper(lua_State *L, V v, int pos) {
    return lua_tonumber(L, pos);
}

template<typename T>
int cpp_table_container_set_normal(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_container_set_normal: invalid container");
        return 0;
    }
    auto container = gLuaContainerHolder.Get(pointer);
    if (!container) {
        luaL_error(L, "cpp_table_container_set_normal: no container found %p", pointer);
        return 0;
    }
    bool ret = false;
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    if constexpr (std::is_same<T, int32_t>::value || std::is_same<T, uint32_t>::value ||
                  std::is_same<T, int64_t>::value || std::is_same<T, uint64_t>::value) {
        if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
            luaL_error(L, "cpp_table_container_set_normal: invalid value type %d", lua_type(L, 3));
            return 0;
        }
        T value = lua_tointeger(L, 3);
        ret = container->Set<T>(idx, value, is_nil);
    } else if constexpr (std::is_same<T, bool>::value) {
        if (lua_type(L, 3) != LUA_TBOOLEAN && lua_type(L, 3) != LUA_TNIL) {
            luaL_error(L, "cpp_table_container_set_normal: invalid value type %d", lua_type(L, 3));
            return 0;
        }
        T value = lua_toboolean(L, 3);
        ret = container->Set<T>(idx, value, is_nil);
    } else if constexpr (std::is_same<T, float>::value || std::is_same<T, double>::value) {
        if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
            luaL_error(L, "cpp_table_container_set_normal: invalid value type %d", lua_type(L, 3));
            return 0;
        }
        T value = lua_tonumber(L, 3);
        ret = container->Set<T>(idx, value, is_nil);
    } else {
        luaL_error(L, "cpp_table_container_set_normal: invalid type %s %s", container->GetName().data(),
                   typeid(T).name());
        return 0;
    }
#else
    if (lua_type(L, 3) != lua_get_type<T>(L) && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_container_set_normal: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    T value = lua_get_helper<T>(L, container, 3);
    ret = container->Set<T>(idx, value, is_nil);
#endif
    if (!ret) {
        luaL_error(L, "cpp_table_container_set_normal: %s invalid idx %d", container->GetName().data(), idx);
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
        shared_str = gStringHeap.Add(StringView(str, size));
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
    auto userdata_pointer = lua_newuserdata(L, 0);
    gLuaContainerHolder.SetArray(userdata_pointer, array);
    cpp_table_reg_array_container_userdata(L, array_pointer, array->GetLayoutMember()->key);
    LLOG("cpp_table_container_get_array: %s new %p", array->GetName().data(), array_pointer);
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
    auto userdata_pointer = lua_newuserdata(L, 0);
    gLuaContainerHolder.SetMap(userdata_pointer, map);
    cpp_table_reg_map_container_userdata(L, map_pointer, map->GetLayoutMember()->key, map->GetLayoutMember()->value);
    LLOG("cpp_table_container_get_map: %s new %p", map->GetName().data(), map_pointer);
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
    auto layout_key = gStringHeap.Add(StringView(name, name_size));
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
    auto pointer = lua_newuserdata(L, 0);
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
    LLOG("cpp_table_delete_array_container: %s %p", array->GetName().data(), array.get());
    gLuaContainerHolder.RemoveArray(pointer);
    return 0;
}

template<typename T>
int cpp_table_array_container_get_normal(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_get_normal: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_get_normal: no array found %p", pointer);
        return 0;
    }
    T value = 0;
    bool is_nil = false;
    auto ret = array->Get<T>(idx, value, is_nil);
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_get_normal: %s invalid idx %d", array->GetName().data(), idx);
        return 0;
    }
    if (is_nil) {
        lua_pushnil(L);
        return 1;
    }
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    if constexpr (std::is_same<T, int32_t>::value || std::is_same<T, uint32_t>::value ||
                  std::is_same<T, int64_t>::value || std::is_same<T, uint64_t>::value) {
        lua_pushinteger(L, value);
    } else if constexpr (std::is_same<T, bool>::value) {
        lua_pushboolean(L, value);
    } else if constexpr (std::is_same<T, float>::value || std::is_same<T, double>::value) {
        lua_pushnumber(L, value);
    } else {
        luaL_error(L, "cpp_table_array_container_get_normal: invalid type %s %s", array->GetName().data(),
                   typeid(T).name());
        return 0;
    }
#else
    lua_push_helper(L, array, value);
#endif
    return 1;
}

template<typename T>
int cpp_table_array_container_set_normal(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    int idx = lua_tointeger(L, 2);
    bool is_nil = lua_isnil(L, 3);
    if (!pointer) {
        luaL_error(L, "cpp_table_array_container_set_normal: invalid array");
        return 0;
    }
    auto array = gLuaContainerHolder.GetArray(pointer);
    if (!array) {
        luaL_error(L, "cpp_table_array_container_set_normal: no array found %p", pointer);
        return 0;
    }
    bool ret = false;
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    if constexpr (std::is_same<T, int32_t>::value || std::is_same<T, uint32_t>::value ||
                  std::is_same<T, int64_t>::value || std::is_same<T, uint64_t>::value) {
        if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
            luaL_error(L, "cpp_table_array_container_set_normal: invalid value type %d", lua_type(L, 3));
            return 0;
        }
        T value = lua_tointeger(L, 3);
        ret = array->Set<int32_t>(idx, value, is_nil);
    } else if constexpr (std::is_same<T, bool>::value) {
        if (lua_type(L, 3) != LUA_TBOOLEAN && lua_type(L, 3) != LUA_TNIL) {
            luaL_error(L, "cpp_table_array_container_set_normal: invalid value type %d", lua_type(L, 3));
            return 0;
        }
        T value = lua_toboolean(L, 3);
        ret = array->Set<bool>(idx, value, is_nil);
    } else if constexpr (std::is_same<T, float>::value || std::is_same<T, double>::value) {
        if (lua_type(L, 3) != LUA_TNUMBER && lua_type(L, 3) != LUA_TNIL) {
            luaL_error(L, "cpp_table_array_container_set_normal: invalid value type %d", lua_type(L, 3));
            return 0;
        }
        T value = lua_tonumber(L, 3);
        ret = array->Set<float>(idx, value, is_nil);
    } else {
        luaL_error(L, "cpp_table_array_container_set_normal: invalid type %s %s", array->GetName().data(),
                   typeid(T).name());
        return 0;
    }
#else
    if (lua_type(L, 3) != lua_get_type<T>(L) && lua_type(L, 3) != LUA_TNIL) {
        luaL_error(L, "cpp_table_array_container_set_normal: invalid value type %d", lua_type(L, 3));
        return 0;
    }
    T value = lua_get_helper<T>(L, array, 3);
    ret = array->Set<T>(idx, value, is_nil);
#endif
    if (!ret) {
        luaL_error(L, "cpp_table_array_container_set_normal: %s invalid idx %d", array->GetName().data(), idx);
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
        shared_str = gStringHeap.Add(StringView(str, size));
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
    auto layout_key = gStringHeap.Add(StringView(name, name_size));
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
    auto pointer = lua_newuserdata(L, 0);
    gLuaContainerHolder.SetMap(pointer, map);
    // create map pointer -> userdata pointer mapping in lua _G.CPP_TABLE_MAP_CONTAINER which is a weak table
    cpp_table_reg_map_container_userdata(L, map.get(), layout_member->key, layout_member->value);
    LLOG("cpp_table_create_map_container: %s %p %s %s", map->GetName().data(), map.get(), layout_member->key->data(),
         layout_member->value->data());
    return 1;
}

template<typename K>
Map::MapValue32 map_get_32(MapPtr map, K key, bool &is_nil) {
    static_assert(true, "map_get_32: invalid type");
    return Map::MapValue32();
}

template<>
Map::MapValue32 map_get_32<int32_t>(MapPtr map, int32_t key, bool &is_nil) {
    return map->Get32by32(key, is_nil);
}

template<>
Map::MapValue32 map_get_32<int64_t>(MapPtr map, int64_t key, bool &is_nil) {
    return map->Get32by64(key, is_nil);
}

template<>
Map::MapValue32 map_get_32<StringPtr>(MapPtr map, StringPtr key, bool &is_nil) {
    return map->Get32byString(key, is_nil);
}

template<typename K>
Map::MapValue32 cpp_table_map_container_get_map_value32(MapPtr map, K key, bool &is_nil) {// no data, just return nil
    if (!map->GetMap().m_void) {
        is_nil = true;
        return Map::MapValue32();
    }

    Map::MapValue32 ret;
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    if constexpr (std::is_same<K, int32_t>::value) {
        ret = map->Get32by32(key, is_nil);
    } else if constexpr (std::is_same<K, int64_t>::value) {
        ret = map->Get32by64(key, is_nil);
    } else {
        ret = map->Get32byString(key, is_nil);
    }
#else
    ret = map_get_32<K>(map, key, is_nil);
#endif
    return ret;
}

template<typename K>
Map::MapValue64 map_get_64(MapPtr map, K key, bool &is_nil) {
    static_assert(true, "map_get_64: invalid type");
    return Map::MapValue64();
}

template<>
Map::MapValue64 map_get_64<int32_t>(MapPtr map, int32_t key, bool &is_nil) {
    return map->Get64by32(key, is_nil);
}

template<>
Map::MapValue64 map_get_64<int64_t>(MapPtr map, int64_t key, bool &is_nil) {
    return map->Get64by64(key, is_nil);
}

template<>
Map::MapValue64 map_get_64<StringPtr>(MapPtr map, StringPtr key, bool &is_nil) {
    return map->Get64byString(key, is_nil);
}

template<typename K>
Map::MapValue64 cpp_table_map_container_get_map_value64(MapPtr map, K key, bool &is_nil) {
    if (!map->GetMap().m_void) {
        is_nil = true;
        return Map::MapValue64();
    }

    Map::MapValue64 ret;
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    if constexpr (std::is_same<K, int32_t>::value) {
        ret = map->Get64by32(key, is_nil);
    } else if constexpr (std::is_same<K, int64_t>::value) {
        ret = map->Get64by64(key, is_nil);
    } else {
        ret = map->Get64byString(key, is_nil);
    }
#else
    ret = map_get_64<K>(map, key, is_nil);
#endif
    return ret;
}

template<typename K>
int cpp_table_map_container_get_by(lua_State *L, MapPtr map, K key, int value_message_id) {
    bool is_nil = false;
    switch (value_message_id) {
        case mt_int32: {
            auto ret = cpp_table_map_container_get_map_value32(map, key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushinteger(L, ret.m_32);
            return 1;
        }
        case mt_uint32: {
            auto ret = cpp_table_map_container_get_map_value32(map, key, is_nil);
            if (is_nil) {
                return 1;
            }
            lua_pushinteger(L, ret.m_u32);
            return 1;
        }
        case mt_int64: {
            auto ret = cpp_table_map_container_get_map_value64(map, key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushinteger(L, ret.m_64);
            return 1;
        }
        case mt_uint64: {
            auto ret = cpp_table_map_container_get_map_value64(map, key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushinteger(L, ret.m_u64);
            return 1;
        }
        case mt_float: {
            auto ret = cpp_table_map_container_get_map_value32(map, key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushnumber(L, ret.m_float);
            return 1;
        }
        case mt_double: {
            auto ret = cpp_table_map_container_get_map_value64(map, key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushnumber(L, ret.m_double);
            return 1;
        }
        case mt_bool: {
            auto ret = cpp_table_map_container_get_map_value32(map, key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushboolean(L, ret.m_bool);
            return 1;
        }
        case mt_string: {
            auto ret = cpp_table_map_container_get_map_value64(map, key, is_nil);
            if (is_nil) {
                lua_pushnil(L);
                return 1;
            }
            lua_pushlstring(L, ret.m_string->c_str(), ret.m_string->size());
            return 1;
        }
        default: {
            auto ret = cpp_table_map_container_get_map_value64(map, key, is_nil);
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

    switch (key_message_id) {
        case mt_int32: {
            int32_t key = (int32_t) lua_tointeger(L, 2);
            return cpp_table_map_container_get_by<int32_t>(L, map, (int32_t) key, value_message_id);
        }
        case mt_uint32: {
            uint32_t key = (uint32_t) lua_tointeger(L, 2);
            return cpp_table_map_container_get_by<int32_t>(L, map, (int32_t) key, value_message_id);
        }
        case mt_int64: {
            int64_t key = (int64_t) lua_tointeger(L, 2);
            return cpp_table_map_container_get_by<int64_t>(L, map, (int64_t) key, value_message_id);
        }
        case mt_uint64: {
            uint64_t key = (uint64_t) lua_tointeger(L, 2);
            return cpp_table_map_container_get_by<int64_t>(L, map, (int64_t) key, value_message_id);
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
            return cpp_table_map_container_get_by<int32_t>(L, map, (int32_t) key, value_message_id);
        }
        case mt_string: {
            size_t size = 0;
            const char *str = lua_tolstring(L, 2, &size);
            auto shared_str = gStringHeap.Add(StringView(str, size));
            return cpp_table_map_container_get_by<StringPtr>(L, map, shared_str, value_message_id);
        }
        default: {
            luaL_error(L, "cpp_table_map_container_get: invalid key type %d", key_message_id);
            return 0;
        }
    }
}

template<typename K>
void map_set_32(MapPtr map, K key, Map::MapValue32 value) {
    static_assert(true, "map_set_32: invalid type");
}

template<>
void map_set_32<int32_t>(MapPtr map, int32_t key, Map::MapValue32 value) {
    map->Set32by32(key, value);
}

template<>
void map_set_32<int64_t>(MapPtr map, int64_t key, Map::MapValue32 value) {
    map->Set32by64(key, value);
}

template<>
void map_set_32<StringPtr>(MapPtr map, StringPtr key, Map::MapValue32 value) {
    map->Set32byString(key, value);
}

template<typename K>
void cpp_table_map_container_set_map_value32(MapPtr map, K key, Map::MapValue32 value) {
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    if constexpr (std::is_same<K, int32_t>::value) {
        map->Set32by32(key, value);
    } else if constexpr (std::is_same<K, int64_t>::value) {
        map->Set32by64(key, value);
    } else {
        map->Set32byString(key, value);
    }
#else
    map_set_32<K>(map, key, value);
#endif
}

template<typename K>
void map_set_64(MapPtr map, K key, Map::MapValue64 value) {
    static_assert(true, "map_set_64: invalid type");
}

template<>
void map_set_64<int32_t>(MapPtr map, int32_t key, Map::MapValue64 value) {
    map->Set64by32(key, value);
}

template<>
void map_set_64<int64_t>(MapPtr map, int64_t key, Map::MapValue64 value) {
    map->Set64by64(key, value);
}

template<>
void map_set_64<StringPtr>(MapPtr map, StringPtr key, Map::MapValue64 value) {
    map->Set64byString(key, value);
}

template<typename K>
void cpp_table_map_container_set_map_value64(MapPtr map, K key, Map::MapValue64 value) {
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    if constexpr (std::is_same<K, int32_t>::value) {
        map->Set64by32(key, value);
    } else if constexpr (std::is_same<K, int64_t>::value) {
        map->Set64by64(key, value);
    } else {
        map->Set64byString(key, value);
    }
#else
    map_set_64<K>(map, key, value);
#endif
}

template<typename K>
void map_del_32(MapPtr map, K key) {
    static_assert(true, "map_del_32: invalid type");
}

template<>
void map_del_32<int32_t>(MapPtr map, int32_t key) {
    map->Remove32by32(key);
}

template<>
void map_del_32<int64_t>(MapPtr map, int64_t key) {
    map->Remove32by64(key);
}

template<>
void map_del_32<StringPtr>(MapPtr map, StringPtr key) {
    map->Remove32byString(key);
}

template<typename K>
void cpp_table_map_container_remove_map_value32(MapPtr map, K key) {
    if (!map->GetMap().m_void) {
        return;
    }
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    if constexpr (std::is_same<K, int32_t>::value) {
        map->Remove32by32(key);
    } else if constexpr (std::is_same<K, int64_t>::value) {
        map->Remove32by64(key);
    } else {
        map->Remove32byString(key);
    }
#else
    map_del_32<K>(map, key);
#endif
}

template<typename K>
void map_del_64(MapPtr map, K key) {
    static_assert(true, "map_del_64: invalid type");
}

template<>
void map_del_64<int32_t>(MapPtr map, int32_t key) {
    map->Remove64by32(key);
}

template<>
void map_del_64<int64_t>(MapPtr map, int64_t key) {
    map->Remove64by64(key);
}

template<>
void map_del_64<StringPtr>(MapPtr map, StringPtr key) {
    map->Remove64byString(key);
}

template<typename K>
void cpp_table_map_container_remove_map_value64(MapPtr map, K key) {
    if (!map->GetMap().m_void) {
        return;
    }
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
    if constexpr (std::is_same<K, int32_t>::value) {
        map->Remove64by32(key);
    } else if constexpr (std::is_same<K, int64_t>::value) {
        map->Remove64by64(key);
    } else {
        map->Remove64byString(key);
    }
#else
    map_del_64<K>(map, key);
#endif
}

template<typename K>
void cpp_table_map_container_set_by(lua_State *L, MapPtr map, K key, int value_message_id, bool is_nil) {
    switch (value_message_id) {
        case mt_int32: {
            if (!is_nil) {
                Map::MapValue32 value;
                value.m_32 = lua_tointeger(L, 3);
                cpp_table_map_container_set_map_value32(map, key, value);
            } else {
                cpp_table_map_container_remove_map_value32(map, key);
            }
            break;
        }
        case mt_uint32: {
            if (!is_nil) {
                Map::MapValue32 value;
                value.m_u32 = lua_tointeger(L, 3);
                cpp_table_map_container_set_map_value32(map, key, value);
            } else {
                cpp_table_map_container_remove_map_value32(map, key);
            }
            break;
        }
        case mt_int64: {
            if (!is_nil) {
                Map::MapValue64 value;
                value.m_64 = lua_tointeger(L, 3);
                cpp_table_map_container_set_map_value64(map, key, value);
            } else {
                cpp_table_map_container_remove_map_value64(map, key);
            }
            break;
        }
        case mt_uint64: {
            if (!is_nil) {
                Map::MapValue64 value;
                value.m_u64 = lua_tointeger(L, 3);
                cpp_table_map_container_set_map_value64(map, key, value);
            } else {
                cpp_table_map_container_remove_map_value64(map, key);
            }
            break;
        }
        case mt_float: {
            if (!is_nil) {
                Map::MapValue32 value;
                value.m_float = lua_tonumber(L, 3);
                cpp_table_map_container_set_map_value32(map, key, value);
            } else {
                cpp_table_map_container_remove_map_value32(map, key);
            }
            break;
        }
        case mt_double: {
            if (!is_nil) {
                Map::MapValue64 value;
                value.m_double = lua_tonumber(L, 3);
                cpp_table_map_container_set_map_value64(map, key, value);
            } else {
                cpp_table_map_container_remove_map_value64(map, key);
            }
            break;
        }
        case mt_bool: {
            if (!is_nil) {
                Map::MapValue32 value;
                value.m_bool = lua_toboolean(L, 3);
                cpp_table_map_container_set_map_value32(map, key, value);
            } else {
                cpp_table_map_container_remove_map_value32(map, key);
            }
            break;
        }
        case mt_string: {
            if (!is_nil) {
                size_t size = 0;
                const char *str = lua_tolstring(L, 3, &size);
                auto new_value = gStringHeap.Add(StringView(str, size));

                bool old_is_nil = false;
                auto old_value = cpp_table_map_container_get_map_value64(map, key, old_is_nil);
                if (!old_is_nil) {
                    if (old_value.m_string == new_value.get()) {
                        return;
                    }
                    old_value.m_string->Release();
                }

                new_value->AddRef();

                Map::MapValue64 value;
                value.m_string = new_value.get();
                cpp_table_map_container_set_map_value64(map, key, value);
            } else {
                if (map->GetMap().m_void) {
                    bool old_is_nil = false;
                    auto old_value = cpp_table_map_container_get_map_value64(map, key, old_is_nil);
                    if (!old_is_nil) {
                        old_value.m_string->Release();
                    }
                }
                cpp_table_map_container_remove_map_value64(map, key);
            }
            break;
        }
        default: {
            if (!is_nil) {
                auto new_obj = gLuaContainerHolder.Get(lua_touserdata(L, 3));
                if (!new_obj) {
                    luaL_error(L, "cpp_table_map_container_set: invalid obj");
                    return;
                }

                if (new_obj->GetMessageId() != value_message_id) {
                    luaL_error(L, "cpp_table_map_container_set: invalid obj message_id %d %d", new_obj->GetMessageId(),
                               value_message_id);
                    return;
                }

                bool old_is_nil = false;
                auto old_value = cpp_table_map_container_get_map_value64(map, key, old_is_nil);
                if (!old_is_nil) {
                    if (old_value.m_obj == new_obj.get()) {
                        return;
                    }
                    old_value.m_obj->Release();
                }

                new_obj->AddRef();

                Map::MapValue64 value;
                value.m_obj = new_obj.get();
                cpp_table_map_container_set_map_value64(map, key, value);
            } else {
                bool old_is_nil = false;
                auto old_value = cpp_table_map_container_get_map_value64(map, key, old_is_nil);
                if (!old_is_nil) {
                    old_value.m_obj->Release();
                }
                cpp_table_map_container_remove_map_value64(map, key);
            }
            break;
        }
    }
}

static int cpp_table_map_container_set(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    if (!pointer) {
        luaL_error(L, "cpp_table_map_container_set: invalid map");
        return 0;
    }
    auto map = gLuaContainerHolder.GetMap(pointer);
    if (!map) {
        luaL_error(L, "cpp_table_map_container_set: no map found %p", pointer);
        return 0;
    }
    int key_message_id = lua_tointeger(L, 4);
    int value_message_id = lua_tointeger(L, 5);
    bool is_nil = lua_isnil(L, 3);

    if (key_message_id != map->GetKeyMessageId() || value_message_id != map->GetValueMessageId()) {
        luaL_error(L, "cpp_table_map_container_set: invalid message_id %d %d", key_message_id, value_message_id);
        return 0;
    }

    switch (key_message_id) {
        case mt_int32: {
            int32_t key = (int32_t) lua_tointeger(L, 2);
            cpp_table_map_container_set_by<int32_t>(L, map, (int32_t) key, value_message_id, is_nil);
            return 0;
        }
        case mt_uint32: {
            uint32_t key = (uint32_t) lua_tointeger(L, 2);
            cpp_table_map_container_set_by<int32_t>(L, map, (int32_t) key, value_message_id, is_nil);
            return 0;
        }
        case mt_int64: {
            int64_t key = (int64_t) lua_tointeger(L, 2);
            cpp_table_map_container_set_by<int64_t>(L, map, (int64_t) key, value_message_id, is_nil);
            return 0;
        }
        case mt_uint64: {
            uint64_t key = (uint64_t) lua_tointeger(L, 2);
            cpp_table_map_container_set_by<int64_t>(L, map, (int64_t) key, value_message_id, is_nil);
            return 0;
        }
        case mt_float: {
            luaL_error(L, "cpp_table_map_container_set: invalid key type %d", key_message_id);
            return 0;
        }
        case mt_double: {
            luaL_error(L, "cpp_table_map_container_set: invalid key type %d", key_message_id);
            return 0;
        }
        case mt_bool: {
            bool key = (bool) lua_toboolean(L, 2);
            cpp_table_map_container_set_by<int32_t>(L, map, (int32_t) key, value_message_id, is_nil);
            return 0;
        }
        case mt_string: {
            size_t size = 0;
            const char *str = lua_tolstring(L, 2, &size);
            auto shared_str = gStringHeap.Add(StringView(str, size));
            cpp_table_map_container_set_by<StringPtr>(L, map, shared_str, value_message_id, is_nil);
            return 0;
        }
        default: {
            luaL_error(L, "cpp_table_map_container_set: invalid key type %d", key_message_id);
            return 0;
        }
    }
}

static int cpp_table_delete_map_container(lua_State *L) {
    auto pointer = lua_touserdata(L, 1);
    if (!pointer) {
        luaL_error(L, "cpp_table_delete_map_container: invalid pointer");
        return 0;
    }
    auto map = gLuaContainerHolder.GetMap(pointer);
    if (!map) {
        luaL_error(L, "cpp_table_delete_map_container: no map found %p", pointer);
        return 0;
    }
    LLOG("cpp_table_delete_map_container: %s %p", map->GetName().data(), map.get());
    gLuaContainerHolder.RemoveMap(pointer);
    return 0;
}

}

std::vector<luaL_Reg> GetCppTableFuncs() {
    return {
            {"cpp_table_set_message_id",             cpp_table::cpp_table_set_message_id},
            {"cpp_table_update_layout",              cpp_table::cpp_table_update_layout},
            {"cpp_table_dump_statistic",             cpp_table::cpp_table_dump_statistic},

            {"cpp_table_create_container",           cpp_table::cpp_table_create_container},
            {"cpp_table_delete_container",           cpp_table::cpp_table_delete_container},
            {"cpp_table_container_get_int32",        cpp_table::cpp_table_container_get_normal<int32_t>},
            {"cpp_table_container_set_int32",        cpp_table::cpp_table_container_set_normal<int32_t>},
            {"cpp_table_container_get_uint32",       cpp_table::cpp_table_container_get_normal<uint32_t>},
            {"cpp_table_container_set_uint32",       cpp_table::cpp_table_container_set_normal<uint32_t>},
            {"cpp_table_container_get_int64",        cpp_table::cpp_table_container_get_normal<int64_t>},
            {"cpp_table_container_set_int64",        cpp_table::cpp_table_container_set_normal<int64_t>},
            {"cpp_table_container_get_uint64",       cpp_table::cpp_table_container_get_normal<uint64_t>},
            {"cpp_table_container_set_uint64",       cpp_table::cpp_table_container_set_normal<uint64_t>},
            {"cpp_table_container_get_float",        cpp_table::cpp_table_container_get_normal<float>},
            {"cpp_table_container_set_float",        cpp_table::cpp_table_container_set_normal<float>},
            {"cpp_table_container_get_double",       cpp_table::cpp_table_container_get_normal<double>},
            {"cpp_table_container_set_double",       cpp_table::cpp_table_container_set_normal<double>},
            {"cpp_table_container_get_bool",         cpp_table::cpp_table_container_get_normal<bool>},
            {"cpp_table_container_set_bool",         cpp_table::cpp_table_container_set_normal<bool>},
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
            {"cpp_table_array_container_get_int32",  cpp_table::cpp_table_array_container_get_normal<int32_t>},
            {"cpp_table_array_container_set_int32",  cpp_table::cpp_table_array_container_set_normal<int32_t>},
            {"cpp_table_array_container_get_uint32", cpp_table::cpp_table_array_container_get_normal<uint32_t>},
            {"cpp_table_array_container_set_uint32", cpp_table::cpp_table_array_container_set_normal<uint32_t>},
            {"cpp_table_array_container_get_int64",  cpp_table::cpp_table_array_container_get_normal<int64_t>},
            {"cpp_table_array_container_set_int64",  cpp_table::cpp_table_array_container_set_normal<int64_t>},
            {"cpp_table_array_container_get_uint64", cpp_table::cpp_table_array_container_get_normal<uint64_t>},
            {"cpp_table_array_container_set_uint64", cpp_table::cpp_table_array_container_set_normal<uint64_t>},
            {"cpp_table_array_container_get_float",  cpp_table::cpp_table_array_container_get_normal<float>},
            {"cpp_table_array_container_set_float",  cpp_table::cpp_table_array_container_set_normal<float>},
            {"cpp_table_array_container_get_double", cpp_table::cpp_table_array_container_get_normal<double>},
            {"cpp_table_array_container_set_double", cpp_table::cpp_table_array_container_set_normal<double>},
            {"cpp_table_array_container_get_bool",   cpp_table::cpp_table_array_container_get_normal<bool>},
            {"cpp_table_array_container_set_bool",   cpp_table::cpp_table_array_container_set_normal<bool>},
            {"cpp_table_array_container_get_string", cpp_table::cpp_table_array_container_get_string},
            {"cpp_table_array_container_set_string", cpp_table::cpp_table_array_container_set_string},
            {"cpp_table_array_container_get_obj",    cpp_table::cpp_table_array_container_get_obj},
            {"cpp_table_array_container_set_obj",    cpp_table::cpp_table_array_container_set_obj},

            {"cpp_table_create_map_container",       cpp_table::cpp_table_create_map_container},
            {"cpp_table_map_container_get",          cpp_table::cpp_table_map_container_get},
            {"cpp_table_map_container_set",          cpp_table::cpp_table_map_container_set},
            {"cpp_table_delete_map_container",       cpp_table::cpp_table_delete_map_container},
    };
}
