#include "core.h"

Global g_global;

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

//////////////////////////////////lua2cpp begin//////////////////////////////////

Any::~Any() {
    if (type == Type::TABLE) {
        delete value.table;
    }
    magic = 0;
}

Table::~Table() {
    for (auto &it: map) {
        delete it.first;
        delete it.second;
    }
    for (auto &it: array) {
        delete it.first;
        delete it.second;
    }
}

Any *Table::Get(Any *key) {
    if (is_array) {
        lua_Integer idx = -1;
        if (key->type == Type::INTEGER) {
            idx = key->value.integer;
        } else if (key->type == Type::NUMBER && is_integer(key->value.number)) {
            idx = key->value.number;
        }

        if (idx >= 1 && idx <= (int) array.size()) {
            return array[idx - 1].second;
        }
    } else {
        auto it = map.find(key);
        if (it != map.end()) {
            return it->second;
        }
    }
    return 0;
}

void Table::Set(Any *key, Any *value) {
    if (is_array) {
        lua_Integer idx = -1;
        if (key->type == Type::INTEGER) {
            idx = key->value.integer;
        } else if (key->type == Type::NUMBER && is_integer(key->value.number)) {
            idx = key->value.number;
        }

        if (idx >= 1 && idx <= (int) array.size()) {
            auto old = array[idx - 1];
            delete old.first;
            delete old.second;
            array[idx - 1] = std::make_pair(key, value);
            return;
        } else if (idx == (int) array.size() + 1) {   // 按顺序插入才会保持数组
            array.push_back(std::make_pair(key, value));
            return;
        }

        // 转成map
        is_array = false;
        for (auto &it: array) {
            map[it.first] = it.second;
        }
        array.clear();
    }

    auto it = map.find(key);
    if (it != map.end()) {
        auto old = it->second;
        delete old;
        it->second = value;
    } else {
        map[key] = value;
    }
}

std::string Any::Dump(StringHeap *stringHeap, int tab) {
    std::string ret;
    switch (type) {
        case Type::NIL:
            ret = "nil";
            break;
        case Type::NUMBER:
            ret = std::to_string(value.number);
            break;
        case Type::INTEGER:
            ret = std::to_string(value.integer);
            break;
        case Type::STRING:
            ret = "\"" + stringHeap->GetString(value.string_idx) + "\"";
            break;
        case Type::BOOLEAN:
            ret = value.boolean ? "true" : "false";
            break;
        case Type::TABLE: {
            if (value.table->is_array) {
                ret = "{ --array\n";
                for (auto &it: value.table->array) {
                    ret += std::string(tab + 1, ' ') + "[" + it.first->Dump(stringHeap, tab + 1) + "] = " +
                           it.second->Dump(stringHeap, tab + 1) + ",\n";
                }
            } else {
                ret = "{ --map\n";
                for (auto &it: value.table->map) {
                    ret += std::string(tab + 1, ' ') + "[" + it.first->Dump(stringHeap, tab + 1) + "] = " +
                           it.second->Dump(stringHeap, tab + 1) + ",\n";
                }
            }
            ret += std::string(tab, ' ') + "}";
            break;
        }
        default:
            ret = "unknown type " + std::to_string((int) type);
    }
    return ret;
}

int Any::LuaToAny(lua_State *L, int index, Any *any, StringHeap *stringHeap) {
    index = lua_absindex(L, index);
    int type = lua_type(L, index);

    switch (type) {
        case LUA_TNIL:
            any->type = Type::NIL;
            LLOG("LuaToAny: index %d NIL", index);
            break;
        case LUA_TNUMBER:
            if (lua_isinteger(L, index)) {
                any->type = Type::INTEGER;
                any->value.integer = lua_tointeger(L, index);
                LLOG("LuaToAny: index %d INTEGER %lld", index, any->value.integer);
            } else {
                any->type = Type::NUMBER;
                any->value.number = lua_tonumber(L, index);
                LLOG("LuaToAny: index %d NUMBER %f", index, any->value.number);
            }
            break;
        case LUA_TBOOLEAN:
            any->type = Type::BOOLEAN;
            any->value.boolean = lua_toboolean(L, index);
            LLOG("LuaToAny: index %d BOOLEAN %d", index, any->value.boolean);
            break;
        case LUA_TSTRING: {
            any->type = Type::STRING;
            size_t size = 0;
            const char *str = lua_tolstring(L, index, &size);
            any->value.string_idx = stringHeap->AddString(std::string(str, size));
            LLOG("LuaToAny: index %d STRING %s string_idx %d", index,
                 stringHeap->GetString(any->value.string_idx).c_str(), any->value.string_idx);
            break;
        }
        case LUA_TTABLE: {
            LLOG("LuaToAny: index %d TABLE", index);
            lua_checkstack(L, 1);
            any->type = Type::TABLE;
            any->value.table = new Table();
            any->value.table->string_heap = stringHeap;
            lua_pushnil(L);
            while (lua_next(L, index) != 0) {
                Any *key = new Any();
                Any *value = new Any();
                auto ret = LuaToAny(L, index + 1, key, stringHeap);
                if (ret != 0) {
                    LERR("LuaToAny: LuaToAny failed key");
                    return -1;
                }
                ret = LuaToAny(L, index + 2, value, stringHeap);
                if (ret != 0) {
                    LERR("LuaToAny: LuaToAny failed value");
                    return -1;
                }
                lua_pop(L, 1);

                any->value.table->Set(key, value);
            }
            break;
        }
        default:
            LERR("LuaToAny: unknown type %d", type);
            return -1;
    }
    return 0;
}

int Any::PushLuaValue(lua_State *L, Any *any, StringHeap *stringHeap) {
    switch (any->type) {
        case Type::NIL:
            lua_pushnil(L);
            break;
        case Type::NUMBER:
            lua_pushnumber(L, any->value.number);
            break;
        case Type::INTEGER:
            lua_pushinteger(L, any->value.integer);
            break;
        case Type::STRING: {
            const std::string &str = stringHeap->GetString(any->value.string_idx);
            lua_pushlstring(L, str.c_str(), str.length());
            break;
        }
        case Type::BOOLEAN:
            lua_pushboolean(L, any->value.boolean);
            break;
        case Type::TABLE: {
            lua_pushlightuserdata(L, any);
            break;
        }
        default:
            LERR("PushLuaValue: unknown type %d", (int) any->type);
            return -1;
    }
    return 0;
}

int StringHeap::AddString(const std::string &str) {
    auto it = m_string_map_cache.find(str);
    if (it != m_string_map_cache.end()) {
        return it->second;
    }

    int idx = m_strings.size();
    m_strings.push_back(str);
    m_string_map_cache[str] = idx;
    return idx;
}

const std::string &StringHeap::GetString(int idx) {
    if (idx < 0 || idx >= (int) m_strings.size()) {
        static std::string empty = "ERROR:invalid string idx";
        return empty;
    }
    return m_strings[idx];
}

Mem::Mem() {
    m_value = new Any();
}

Mem::~Mem() {
    delete m_value;
}

static int table_to_cpp(lua_State *L) {
    const char *table_name = lua_tostring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    Mem *mem = new Mem();

    auto ret = Any::LuaToAny(L, 2, mem->GetValue(), mem->GetStringHeap());
    if (ret != 0) {
        delete mem;
        LERR("table_to_cpp: LuaToAny failed %s", table_name);
        return 0;
    }

    g_global.SetMem(table_name, mem);
    LLOG("table_to_cpp ok: %s %zu", table_name, mem->GetStringHeap()->Size());

    Any::PushLuaValue(L, mem->GetValue(), mem->GetStringHeap());
    return 1;
}

static int dump_cpp_table(lua_State *L) {
    const char *table_name = lua_tostring(L, 1);

    auto mem = g_global.GetMem(table_name);
    if (mem == nullptr || mem->GetValue() == nullptr) {
        LERR("dump_cpp_table: GetMem failed %s", table_name);
        return 0;
    }

    auto str = mem->GetValue()->Dump(mem->GetStringHeap());

    lua_pushlstring(L, str.c_str(), str.length());
    return 1;
}

void Global::SetMem(const std::string &key, Mem *mem) {
    auto it = m_mem.find(key);
    if (it != m_mem.end()) {
        delete it->second;
        it->second = mem;
        LLOG("SetMem Update: %s", key.c_str());
    } else {
        m_mem[key] = mem;
        LLOG("SetMem New: %s", key.c_str());
    }
}

Mem *Global::GetMem(const std::string &key) {
    auto it = m_mem.find(key);
    if (it != m_mem.end()) {
        return it->second;
    }
    return nullptr;
}

static Any *check_cpp_table(lua_State *L, int idx) {
    auto any = (Any *) lua_touserdata(L, 1);
    if (!any) {
        luaL_error(L, "Cpp Table is nullptr");
        return 0;
    }

    if (any->magic != MAGIC_NUMBER) {
        luaL_error(L, "Cpp Table is invalid");
        return 0;
    }

    if (any->type != Type::TABLE) {
        luaL_error(L, "Cpp Table is not table");
        return 0;
    }

    return any;
}

static bool is_base_lua_type(lua_State *L, int idx) {
    int type = lua_type(L, idx);
    return type == LUA_TNIL || type == LUA_TNUMBER || type == LUA_TBOOLEAN || type == LUA_TSTRING;
}

static int index_cpp_table(lua_State *L) {
    auto any = check_cpp_table(L, 1);

    if (!is_base_lua_type(L, 2)) {
        return luaL_error(L, "Cpp Table index key is not base lua type");
    }

    Any key;
    if (Any::LuaToAny(L, 2, &key, any->value.table->string_heap) != 0) {
        return luaL_error(L, "Cpp Table LuaToAny failed");
    }

    auto value = any->value.table->Get(&key);
    if (value) {
        if (Any::PushLuaValue(L, value, any->value.table->string_heap) != 0) {
            return luaL_error(L, "Cpp Table PushLuaValue failed");
        }
        return 1;
    }
    return 0;
}

static int len_cpp_table(lua_State *L) {
    auto any = check_cpp_table(L, 1);
    if (any->value.table->is_array) {
        lua_pushinteger(L, any->value.table->array.size());
    } else {
        lua_pushinteger(L, any->value.table->map.size());
    }
    return 1;
}

static int nextkey_cpp_table(lua_State *L) {
    auto any = check_cpp_table(L, 1);

    Any *next_key = 0;
    if (any->value.table->is_array) {
        lua_Integer idx = -1;
        if (lua_isnil(L, 2)) {
            idx = 1;
        } else if (lua_isinteger(L, 2)) {
            idx = lua_tointeger(L, 2) + 1;
        } else {
            // no such key
            return 0;
        }

        if (idx >= 1 && idx <= (int) any->value.table->array.size()) {
            next_key = any->value.table->array[idx - 1].first;
        } else {
            // out of range
            return 0;
        }
    } else {
        if (lua_isnil(L, 2)) {
            auto it = any->value.table->map.begin();
            if (it != any->value.table->map.end()) {
                next_key = it->first;
            } else {
                // empty
                return 0;
            }
        } else {
            Any tmp_key;
            if (Any::LuaToAny(L, 2, &tmp_key, any->value.table->string_heap) != 0) {
                luaL_error(L, "Cpp Table LuaToAny failed");
                return 0;
            }
            auto it = any->value.table->map.find(&tmp_key);
            if (it != any->value.table->map.end()) {
                it++;
                if (it != any->value.table->map.end()) {
                    next_key = it->first;
                } else {
                    // no next key
                    return 0;
                }
            } else {
                // no such key
                return 0;
            }
        }
    }

    if (Any::PushLuaValue(L, next_key, any->value.table->string_heap) != 0) {
        return luaL_error(L, "Cpp Table PushLuaValue failed");
    }
    return 1;
}

//////////////////////////////////lua2cpp end//////////////////////////////////

//////////////////////////////////memory-walker begin//////////////////////////////////

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

//////////////////////////////////memory-walker end//////////////////////////////////

//////////////////////////////////quick-archiver begin//////////////////////////////////

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
    m_buffer = new char[size];
    m_lz_buffer = new char[size];
    m_buffer_size = size;
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
        int lz_size = LZ4_compress_default(m_buffer + 1, m_lz_buffer + 1, m_pos - 1, m_buffer_size - 1);
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

//////////////////////////////////quick-archiver end//////////////////////////////////

extern "C" int luaopen_libmluacore(lua_State *L) {
    luaL_Reg l[] = {
//////////////////////////////////lua2cpp begin//////////////////////////////////
            {"table_to_cpp",                       table_to_cpp},
            {"dump_cpp_table",                     dump_cpp_table},
            {"index_cpp_table",                    index_cpp_table},
            {"len_cpp_table",                      len_cpp_table},
            {"nextkey_cpp_table",                  nextkey_cpp_table},
//////////////////////////////////lua2cpp end//////////////////////////////////

//////////////////////////////////memory-walker begin//////////////////////////////////
            {"roaring64map_add",                   roaring64map_add},
            {"roaring64map_addchecked",            roaring64map_addchecked},
            {"roaring64map_remove",                roaring64map_remove},
            {"roaring64map_removechecked",         roaring64map_removechecked},
            {"roaring64map_contains",              roaring64map_contains},
            {"roaring64map_clear",                 roaring64map_clear},
            {"roaring64map_maximum",               roaring64map_maximum},
            {"roaring64map_minimum",               roaring64map_minimum},
            {"roaring64map_cardinality",           roaring64map_cardinality},
            {"roaring64map_tostring",              roaring64map_tostring},
            {"roaring64map_bytesize",              roaring64map_bytesize},
//////////////////////////////////memory-walker end//////////////////////////////////

//////////////////////////////////quick-archiver begin//////////////////////////////////
            {"quick_archiver_save",                quick_archiver_save},
            {"quick_archiver_load",                quick_archiver_load},
            {"quick_archiver_set_lz_threshold",    quick_archiver_set_lz_threshold},
            {"quick_archiver_set_max_buffer_size", quick_archiver_set_max_buffer_size},
//////////////////////////////////quick-archiver end//////////////////////////////////

            {nullptr,                              nullptr}
    };
    luaL_newlib(L, l);
    return 1;
}
