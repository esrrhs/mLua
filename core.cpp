#include "core.h"

void llog(const char *header, const char *file, const char *func, int pos, const char *fmt, ...) {
    FILE *pLog = NULL;
    time_t clock1;
    struct tm *tptr;
    va_list ap;

    pLog = fopen("mlua.log", "a+");
    if (pLog == NULL) {
        return;
    }

    clock1 = time(0);
    tptr = localtime(&clock1);

    struct timeval tv;
    gettimeofday(&tv, NULL);

    fprintf(pLog, "===========================[%d.%d.%d, %d.%d.%d %llu]%s:%d,%s:===========================\n%s",
            tptr->tm_year + 1990, tptr->tm_mon + 1,
            tptr->tm_mday, tptr->tm_hour, tptr->tm_min,
            tptr->tm_sec, (long long) ((tv.tv_sec) * 1000 + (tv.tv_usec) / 1000), file, pos, func, header);

    va_start(ap, fmt);
    vfprintf(pLog, fmt, ap);
    fprintf(pLog, "\n\n");
    va_end(ap);

    va_start(ap, fmt);
    vprintf(fmt, ap);
    printf("\n\n");
    va_end(ap);

    fclose(pLog);
}

//////////////////////////////////cpp-table begin//////////////////////////////////
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

//////////////////////////////////cpp-table end//////////////////////////////////

//////////////////////////////////memory-walker begin//////////////////////////////////
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
//////////////////////////////////memory-walker end//////////////////////////////////

//////////////////////////////////quick-archiver begin//////////////////////////////////
namespace quick_archiver {

static QuickArchiver gQuickArchiver;

QuickArchiver::QuickArchiver() {
    m_buffer_size = 16 * 1024 * 1024;
    SetMaxBufferSize(m_buffer_size);
}

QuickArchiver::~QuickArchiver() {
    if (m_buffer) {
        delete[] m_buffer;
        m_buffer = 0;
    }
    if (m_lz_buffer) {
        delete[] m_lz_buffer;
        m_lz_buffer = 0;
    }
}

void QuickArchiver::SetMaxBufferSize(size_t size) {
    if (m_buffer) {
        delete[] m_buffer;
    }
    if (m_lz_buffer) {
        delete[] m_lz_buffer;
    }
    m_buffer_size = size;
    m_buffer = new char[m_buffer_size];
    m_lz_buffer_size = 1 + LZ4_compressBound(size - 1);
    m_lz_buffer = new char[m_lz_buffer_size];
}

int QuickArchiver::Save(lua_State *L) {
    m_saved_string.clear();
    m_pos = 0;
    m_buffer[m_pos] = 'N';
    m_pos += sizeof(char);
    m_table_depth = 0;
    int64_t int_value = 0;
    if (!SaveValue(L, 1, int_value)) {
        return 0;
    }
    if (m_lz_threshold > 0 && m_pos > m_lz_threshold) {
        // lz compress
        int lz_size = LZ4_compress_fast(m_buffer + 1, m_lz_buffer + 1, m_pos - 1, m_lz_buffer_size - 1,
                                        m_lz_acceleration);
        if (lz_size > 0 && lz_size < (int) m_pos - 1) {
            m_lz_buffer[0] = 'Z';
            lua_pushlstring(L, m_lz_buffer, lz_size + 1);
        } else {
            LERR("Save: lz compress failed");
            lua_pushlstring(L, m_buffer, m_pos);
        }
    } else {
        lua_pushlstring(L, m_buffer, m_pos);
    }
    return 1;
}

int QuickArchiver::Load(lua_State *L) {
    size_t size = 0;
    const char *data = lua_tolstring(L, 1, &size);
    if (size == 0) {
        LERR("Load: empty data");
        return 0;
    }
    bool lz = false;
    if (data[0] == 'Z') {
        lz = true;
    } else if (data[0] != 'N') {
        LERR("Load: unknown data type %c", data[0]);
        return 0;
    }

    data++;
    size--;

    if (lz) {
        int lz_size = LZ4_decompress_safe(data, m_buffer, size, m_buffer_size);
        if (lz_size <= 0) {
            LERR("Load: lz decompress failed");
            return 0;
        }
        size = lz_size;
    } else {
        memcpy(m_buffer, data, size);
    }

    m_loaded_string.clear();
    m_pos = 0;
    if (!LoadValue(L, true)) {
        return 0;
    }

    return 1;
}

#define FIND_INT_LEN(v) ((int64_t)v >= INT8_MIN && (int64_t)v <= INT8_MAX) ? sizeof(int8_t) : \
                      ((int64_t)v >= INT16_MIN && (int64_t)v <= INT16_MAX) ? sizeof(int16_t) : \
                      ((int64_t)v >= INT32_MIN && (int64_t)v <= INT32_MAX) ? sizeof(int32_t) : sizeof(int64_t)

#define LOAD_INT(v, len) { \
    switch (len) {         \
          case sizeof(int8_t): { \
                int8_t v8 = 0; \
                memcpy(&v8, &m_buffer[m_pos], sizeof(int8_t)); \
                v = v8; \
                m_pos += sizeof(int8_t); \
                break; \
          } \
          case sizeof(int16_t): { \
                int16_t v16 = 0; \
                memcpy(&v16, &m_buffer[m_pos], sizeof(int16_t)); \
                v = v16; \
                m_pos += sizeof(int16_t); \
                break; \
          } \
          case sizeof(int32_t): { \
                int32_t v32 = 0; \
                memcpy(&v32, &m_buffer[m_pos], sizeof(int32_t)); \
                v = v32; \
                m_pos += sizeof(int32_t); \
                break; \
          } \
          case sizeof(int64_t): {\
                int64_t v64 = 0; \
                memcpy(&v64, &m_buffer[m_pos], sizeof(int64_t)); \
                v = v64; \
                m_pos += sizeof(int64_t); \
                break; \
          } \
          default: \
                LERR("LoadInt: unknown len %d", len); \
                return false; \
     } \
}

#define SAVE_INT(v, len) { \
    switch (len) {         \
          case sizeof(int8_t): { \
                int8_t v8 = (int8_t) v; \
                memcpy(&m_buffer[m_pos], &v8, sizeof(int8_t)); \
                m_pos += sizeof(int8_t); \
                break; \
          } \
          case sizeof(int16_t): { \
                int16_t v16 = (int16_t) v; \
                memcpy(&m_buffer[m_pos], &v16, sizeof(int16_t)); \
                m_pos += sizeof(int16_t); \
                break; \
          } \
          case sizeof(int32_t): { \
                int32_t v32 = (int32_t) v; \
                memcpy(&m_buffer[m_pos], &v32, sizeof(int32_t)); \
                m_pos += sizeof(int32_t); \
                break; \
          } \
          case sizeof(int64_t): {\
                int64_t v64 = (int64_t) v; \
                memcpy(&m_buffer[m_pos], &v64, sizeof(int64_t)); \
                m_pos += sizeof(int64_t); \
                break; \
          } \
          default: \
                LERR("SaveInt: unknown len %d", len); \
                return false; \
     } \
}

bool QuickArchiver::LoadValue(lua_State *L, bool can_be_nil) {
    if (!lua_checkstack(L, 1)) {
        LERR("LoadValue: lua_checkstack failed");
        return false;
    }

    if (m_pos >= m_buffer_size) {
        LERR("LoadValue: buffer overflow");
        return false;
    }

    char type = m_buffer[m_pos];
    m_pos += sizeof(char);

    Type low_type = (Type) (type & 0x0F);

    switch (low_type) {
        case Type::nil:
            if (!can_be_nil) {
                LERR("LoadValue: nil not allowed");
                return false;
            }
            lua_pushnil(L);
            return true;
        case Type::number: {
            if (m_pos + sizeof(double) > m_buffer_size) {
                LERR("LoadValue: buffer overflow");
                return false;
            }
            double v = 0;
            memcpy(&v, &m_buffer[m_pos], sizeof(double));
            m_pos += sizeof(double);
            lua_pushnumber(L, v);
            return true;
        }
        case Type::integer: {
            int len = (type >> 4) & 0x0F;
            if (m_pos + len > m_buffer_size) {
                LERR("LoadValue: buffer overflow");
                return false;
            }
            int64_t v = 0;
            LOAD_INT(v, len);
            lua_pushinteger(L, v);
            return true;
        }
        case Type::bool_true:
            lua_pushboolean(L, 1);
            return true;
        case Type::bool_false:
            lua_pushboolean(L, 0);
            return true;
        case Type::string_idx: {
            int len = (type >> 4) & 0x0F;
            int64_t idx = 0;
            LOAD_INT(idx, len);
            if (idx < 0 || idx >= (int) m_loaded_string.size()) {
                LERR("LoadValue: invalid string idx %d", idx);
                return false;
            }
            lua_pushlstring(L, m_loaded_string[idx].first, m_loaded_string[idx].second);
            return true;
        }
        case Type::string: {
            int len = (type >> 4) & 0x0F;
            int64_t size = 0;
            LOAD_INT(size, len);
            if (m_pos + size > m_buffer_size) {
                LERR("LoadValue: buffer overflow");
                return false;
            }
            m_loaded_string.push_back(std::make_pair(&m_buffer[m_pos], size));
            lua_pushlstring(L, &m_buffer[m_pos], size);
            m_pos += size;
            return true;
        }
        case Type::table_hash: {
            int kv_count = 0;
            memcpy(&kv_count, &m_buffer[m_pos], sizeof(int));
            m_pos += sizeof(int);
            lua_createtable(L, 0, kv_count);
            for (int i = 0; i < kv_count; i++) {
                if (!LoadValue(L, false)) {
                    return false;
                }
                if (!LoadValue(L, true)) {
                    return false;
                }
                lua_rawset(L, -3);
            }
            return true;
        }
        case Type::table_array: {
            int kv_count = 0;
            memcpy(&kv_count, &m_buffer[m_pos], sizeof(int));
            m_pos += sizeof(int);
            lua_createtable(L, kv_count, 0);
            for (int i = 0; i < kv_count; i++) {
                if (!LoadValue(L, false)) {
                    return false;
                }
                if (!LoadValue(L, true)) {
                    return false;
                }
                lua_rawset(L, -3);
            }
            return true;
        }
        default: {
            LERR("LoadValue: unknown type %d", (int) type);
            return false;
        }
    }
}

bool QuickArchiver::SaveValue(lua_State *L, int idx, int64_t &int_value) {
    int type = lua_type(L, idx);
    switch (type) {
        case LUA_TNIL: {
            if (m_pos + sizeof(char) > m_buffer_size) {
                LERR("SaveNil: buffer overflow");
                return false;
            }
            m_buffer[m_pos] = (char) Type::nil;
            m_pos += sizeof(char);
            return true;
        }
        case LUA_TNUMBER:
            if (lua_isinteger(L, idx)) {
                int64_t v = lua_tointeger(L, idx);
                int_value = v;
                int len = FIND_INT_LEN(v);
                if (m_pos + sizeof(char) + len > m_buffer_size) {
                    LERR("SaveInteger: buffer overflow");
                    return false;
                }
                m_buffer[m_pos] = ((char) Type::integer) | ((char) len << 4);
                m_pos += sizeof(char);
                SAVE_INT(v, len);
                return true;
            } else {
                double v = lua_tonumber(L, idx);
                if (m_pos + sizeof(char) + sizeof(double) > m_buffer_size) {
                    LERR("SaveNumber: buffer overflow");
                    return false;
                }
                m_buffer[m_pos] = (char) Type::number;
                memcpy(&m_buffer[m_pos + sizeof(char)], &v, sizeof(double));
                m_pos += sizeof(char) + sizeof(double);
                return true;
            }
        case LUA_TBOOLEAN: {
            bool v = lua_toboolean(L, idx);
            if (m_pos + sizeof(char) > m_buffer_size) {
                LERR("SaveBool: buffer overflow");
                return false;
            }
            m_buffer[m_pos] = v ? (char) Type::bool_true : (char) Type::bool_false;
            m_pos += sizeof(char);
            return true;
        }
        case LUA_TSTRING: {
            size_t size = 0;
            const char *str = lua_tolstring(L, idx, &size);
            auto it = m_saved_string.find(str);
            if (it != m_saved_string.end()) {
                int idx = it->second;
                int idx_len = FIND_INT_LEN(idx);
                if (m_pos + sizeof(char) + idx_len > m_buffer_size) {
                    LERR("SaveSharedString: buffer overflow");
                    return false;
                }
                m_buffer[m_pos] = (char) Type::string_idx | ((char) idx_len << 4);
                m_pos += sizeof(char);
                SAVE_INT(idx, idx_len);
                return true;
            } else {
                int size_len = FIND_INT_LEN(size);
                if (m_pos + sizeof(char) + size_len + size > m_buffer_size) {
                    LERR("SaveString: buffer overflow");
                    return false;
                }
                m_buffer[m_pos] = (char) Type::string | ((char) size_len << 4);
                m_pos += sizeof(char);
                SAVE_INT(size, size_len);
                memcpy(&m_buffer[m_pos], str, size);
                m_pos += size;
                int str_idx = m_saved_string.size();
                m_saved_string[str] = str_idx;
                return true;
            }
        }
        case LUA_TTABLE: {
            m_table_depth++;
            if (m_table_depth > MAX_TABLE_DEPTH) {
                LERR("SaveTable: table depth overflow");
                return false;
            }

            int top = lua_gettop(L);
            if (idx < 0 && -idx <= top) {
                idx = idx + top + 1;
            }

            if (m_pos + sizeof(char) + sizeof(int) > m_buffer_size) {
                LERR("SaveTable: buffer overflow");
                return false;
            }
            int table_begin = m_pos;
            m_pos += sizeof(char) + sizeof(int);

            int kv_count = 0;
            int64_t max_int_key = 0;

            lua_pushnil(L);
            while (lua_next(L, idx) != 0) {
                int64_t key_int = 0;
                if (!SaveValue(L, -2, key_int)) {
                    return false;
                }
                if (key_int > max_int_key) {
                    max_int_key = key_int;
                }
                int64_t value_int = 0;
                if (!SaveValue(L, -1, value_int)) {
                    return false;
                }
                lua_pop(L, 1);
                kv_count++;
            }

            m_buffer[table_begin] = max_int_key == kv_count ? (char) Type::table_array : (char) Type::table_hash;
            memcpy(&m_buffer[table_begin + sizeof(char)], &kv_count, sizeof(int));

            m_table_depth--;

            return true;
        }
        default:
            LERR("SaveValue: unknown type %d", type);
            return false;
    }
}

static int quick_archiver_save(lua_State *L) {
    return gQuickArchiver.Save(L);
}

static int quick_archiver_load(lua_State *L) {
    return gQuickArchiver.Load(L);
}

static int quick_archiver_set_lz_threshold(lua_State *L) {
    size_t size = lua_tointeger(L, 1);
    gQuickArchiver.SetLzThreshold(size);
    return 0;
}

static int quick_archiver_set_max_buffer_size(lua_State *L) {
    size_t size = lua_tointeger(L, 1);
    gQuickArchiver.SetMaxBufferSize(size);
    return 0;
}

static int quick_archiver_set_lz_acceleration(lua_State *L) {
    int acceleration = lua_tointeger(L, 1);
    gQuickArchiver.SetLzAcceleration(acceleration);
    return 0;
}

}
//////////////////////////////////quick-archiver end//////////////////////////////////

extern "C" int luaopen_libmluacore(lua_State *L) {
    luaL_Reg l[] = {
//////////////////////////////////lua2cpp begin//////////////////////////////////
            {"cpp_table_create_container",         cpp_table::cpp_table_create_container},
            {"cpp_table_set_meta_table",           cpp_table::cpp_table_set_meta_table},
            {"cpp_table_delete_container",         cpp_table::cpp_table_delete_container},
            {"cpp_table_container_get_int32",      cpp_table::cpp_table_container_get_int32},
            {"cpp_table_container_set_int32",      cpp_table::cpp_table_container_set_int32},
//////////////////////////////////lua2cpp end//////////////////////////////////

//////////////////////////////////memory-walker begin//////////////////////////////////
            {"roaring64map_add",                   memory_walker::roaring64map_add},
            {"roaring64map_addchecked",            memory_walker::roaring64map_addchecked},
            {"roaring64map_remove",                memory_walker::roaring64map_remove},
            {"roaring64map_removechecked",         memory_walker::roaring64map_removechecked},
            {"roaring64map_contains",              memory_walker::roaring64map_contains},
            {"roaring64map_clear",                 memory_walker::roaring64map_clear},
            {"roaring64map_maximum",               memory_walker::roaring64map_maximum},
            {"roaring64map_minimum",               memory_walker::roaring64map_minimum},
            {"roaring64map_cardinality",           memory_walker::roaring64map_cardinality},
            {"roaring64map_tostring",              memory_walker::roaring64map_tostring},
            {"roaring64map_bytesize",              memory_walker::roaring64map_bytesize},
//////////////////////////////////memory-walker end//////////////////////////////////

//////////////////////////////////quick-archiver begin//////////////////////////////////
            {"quick_archiver_save",                quick_archiver::quick_archiver_save},
            {"quick_archiver_load",                quick_archiver::quick_archiver_load},
            {"quick_archiver_set_lz_threshold",    quick_archiver::quick_archiver_set_lz_threshold},
            {"quick_archiver_set_max_buffer_size", quick_archiver::quick_archiver_set_max_buffer_size},
            {"quick_archiver_set_lz_acceleration", quick_archiver::quick_archiver_set_lz_acceleration},
//////////////////////////////////quick-archiver end//////////////////////////////////

            {nullptr,                              nullptr}
    };
    luaL_newlib(L, l);
    return 1;
}
