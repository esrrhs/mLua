#include "core.h"

const int open_log = 0;
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

        if (idx >= 1 && idx <= array.size()) {
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

        if (idx >= 1 && idx <= array.size()) {
            auto old = array[idx - 1];
            delete old.first;
            delete old.second;
            array[idx - 1] = std::make_pair(key, value);
            return;
        } else if (idx == array.size() + 1) {   // 按顺序插入才会保持数组
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
        
    }
}

int Any::LuaToAny(lua_State *L, int index, Any *any, StringHeap *stringHeap) {
    index = lua_absindex(L, index);

    int type = lua_type(L, index);
    switch (type) {
        case LUA_TNIL:
            any->type = Type::NIL;
            break;
        case LUA_TNUMBER:
            if (lua_isinteger(L, index)) {
                any->type = Type::INTEGER;
                any->value.integer = lua_tointeger(L, index);
            } else {
                any->type = Type::NUMBER;
                any->value.number = lua_tonumber(L, index);
            }
            break;
        case LUA_TBOOLEAN:
            any->type = Type::BOOLEAN;
            any->value.boolean = lua_toboolean(L, index);
            break;
        case LUA_TSTRING: {
            any->type = Type::STRING;
            size_t size = 0;
            const char *str = luaL_tolstring(L, index, &size);
            any->value.string_idx = stringHeap->AddString(std::string(str, size));
            break;
        }
        case LUA_TTABLE:
            any->type = Type::TABLE;
            any->value.table = new Table();
            lua_pushnil(L);
            while (lua_next(L, index) != 0) {
                Any *key = new Any();
                Any *value = new Any();
                auto ret = LuaToAny(L, -2, key, stringHeap);
                if (ret != 0) {
                    LERR("LuaToAny: LuaToAny failed key");
                    return -1;
                }
                ret = LuaToAny(L, -1, value, stringHeap);
                if (ret != 0) {
                    LERR("LuaToAny: LuaToAny failed value");
                    return -1;
                }
                lua_pop(L, 1);

                any->value.table->Set(key, value);
            }
            break;
        default:
            LERR("LuaToAny: unknown type %d", type);
            return -1;
    }
    return 0;
}

uint32_t StringHeap::AddString(const std::string &str) {
    if (str.empty()) {
        return 0;
    }

    auto it = m_string_map.find(str);
    if (it != m_string_map.end()) {
        return it->second;
    }

    uint32_t idx = m_strings.size();
    m_strings.push_back(str);
    m_string_map[str] = idx;
    return idx;
}

const std::string &StringHeap::GetString(uint32_t idx) {
    if (idx >= m_strings.size()) {
        static std::string empty;
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

    auto ret = LuaToAny(L, 2, mem->GetValue(), mem->GetStringHeap());
    if (ret != 0) {
        delete mem;
        LERR("table_to_cpp: LuaToAny failed %s", table_name);
        return 0;
    }

    g_global.SetMem(table_name, mem);
    LLOG("table_to_cpp ok: %s", table_name);

    lua_pushboolean(L, true);
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

extern "C" int luaopen_libmluacore(lua_State *L) {
    luaL_Reg l[] = {
            {"table_to_cpp", table_to_cpp},
            {nullptr,        nullptr}
    };
    luaL_newlib(L, l);
    return 1;
}
