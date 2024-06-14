#include "quick_archiver.h"

namespace quick_archiver {

static QuickArchiver *gQuickArchiver;

static void CheckQuickArchiver() {
    if (!gQuickArchiver) {
        gQuickArchiver = new QuickArchiver();
    }
}

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
    CheckQuickArchiver();
    return gQuickArchiver->Save(L);
}

static int quick_archiver_load(lua_State *L) {
    CheckQuickArchiver();
    return gQuickArchiver->Load(L);
}

static int quick_archiver_set_lz_threshold(lua_State *L) {
    CheckQuickArchiver();
    size_t size = lua_tointeger(L, 1);
    gQuickArchiver->SetLzThreshold(size);
    return 0;
}

static int quick_archiver_set_max_buffer_size(lua_State *L) {
    CheckQuickArchiver();
    size_t size = lua_tointeger(L, 1);
    gQuickArchiver->SetMaxBufferSize(size);
    return 0;
}

static int quick_archiver_set_lz_acceleration(lua_State *L) {
    CheckQuickArchiver();
    int acceleration = lua_tointeger(L, 1);
    gQuickArchiver->SetLzAcceleration(acceleration);
    return 0;
}

}

std::vector<luaL_Reg> GetQuickArchiverFuncs() {
    return {
            {"quick_archiver_save",                quick_archiver::quick_archiver_save},
            {"quick_archiver_load",                quick_archiver::quick_archiver_load},
            {"quick_archiver_set_lz_threshold",    quick_archiver::quick_archiver_set_lz_threshold},
            {"quick_archiver_set_max_buffer_size", quick_archiver::quick_archiver_set_max_buffer_size},
            {"quick_archiver_set_lz_acceleration", quick_archiver::quick_archiver_set_lz_acceleration},
    };
}
