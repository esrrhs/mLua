#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <typeinfo>
#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <unordered_map>
#include <fcntl.h>
#include <sstream>
#include <algorithm>
#include <vector>
#include <unordered_set>
#include <set>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

const int open_debug_log = 0;
const int open_error_log = 1;

#define LLOG(...) if (open_debug_log) {llog("[DEBUG] ", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);}
#define LERR(...) if (open_error_log) {llog("[ERROR] ", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);}

void llog(const char *header, const char *file, const char *func, int pos, const char *fmt, ...);

class Table;

class StringHeap;

enum class Type {
    NIL,
    NUMBER,
    INTEGER,
    STRING,
    BOOLEAN,
    TABLE,
};

#define MAGIC_NUMBER 0x12345678

struct Any {
    ~Any();

    uint32_t magic = MAGIC_NUMBER; // 防止野指针
    Type type = Type::NIL;
    union {
        lua_Number number;
        lua_Integer integer;
        Table *table;
        int string_idx;
        bool boolean;
    } value;

    std::string Dump(StringHeap *stringHeap, int tab = 0);

    static int LuaToAny(lua_State *L, int index, Any *any, StringHeap *stringHeap);

    static int PushLuaValue(lua_State *L, Any *any, StringHeap *stringHeap);
};

struct AnyHash {
    size_t operator()(const Any *a) const {
        switch (a->type) {
            case Type::NIL:
                return 0;
            case Type::NUMBER:
                return std::hash<lua_Number>()(a->value.number);
            case Type::INTEGER:
                return std::hash<lua_Integer>()(a->value.integer);
            case Type::STRING:
                return std::hash<int>()(a->value.string_idx);
            case Type::BOOLEAN:
                return std::hash<bool>()(a->value.boolean);
            case Type::TABLE:
                return std::hash<Table *>()(a->value.table);
        }
        return 0;
    }
};

struct AnyEqual {
    bool operator()(const Any *a, const Any *b) const {
        if (a->type != b->type) {
            if (a->type == Type::NUMBER && b->type == Type::INTEGER) {
                return a->value.number == b->value.integer;
            }
            if (a->type == Type::INTEGER && b->type == Type::NUMBER) {
                return a->value.integer == b->value.number;
            }
            return false;
        }
        switch (a->type) {
            case Type::NIL:
                return true;
            case Type::NUMBER:
                if (std::isnan(a->value.number) && std::isnan(b->value.number)) {
                    return true;
                }
                return a->value.number == b->value.number;
            case Type::INTEGER:
                return a->value.integer == b->value.integer;
            case Type::STRING:
                return a->value.string_idx == b->value.string_idx;
            case Type::BOOLEAN:
                return a->value.boolean == b->value.boolean;
            case Type::TABLE:
                return a->value.table == b->value.table;
        }
        return false;
    }
};

static inline bool is_integer(double value) {
    double intpart;
    return modf(value, &intpart) == 0.0;
}

class StringHeap;

struct Table {
    ~Table();

    bool is_array = true;
    std::vector<std::pair<Any *, Any *>> array;
    std::unordered_map<Any *, Any *, AnyHash, AnyEqual> map;
    StringHeap *string_heap = nullptr;

    Any *Get(Any *key);

    void Set(Any *key, Any *value);
};

class StringHeap {
public:
    int AddString(const std::string &str);

    const std::string &GetString(int idx);

    int GetIndex(const std::string &str);

    size_t Size() {
        return m_strings.size();
    }

private:
    std::vector<std::string> m_strings;
    std::unordered_map<std::string, int> m_string_map_cache;
};

class Mem {
public:
    Mem();

    ~Mem();

    Any *GetValue() {
        return m_value;
    }

    StringHeap *GetStringHeap() {
        return &m_string_heap;
    }

private:
    Any *m_value;
    StringHeap m_string_heap;
};

class Global {
public:
    void SetMem(const std::string &key, Mem *mem);

    Mem *GetMem(const std::string &key);

private:
    std::unordered_map<std::string, Mem *> m_mem;
};
