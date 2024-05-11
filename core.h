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

#include "lz4.h"

#include "roaring.hh"

const int open_debug_log = 0;
const int open_error_log = 1;

#define LLOG(...) if (open_debug_log) {llog("[DEBUG] ", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);}
#define LERR(...) if (open_error_log) {llog("[ERROR] ", __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);}

void llog(const char *header, const char *file, const char *func, int pos, const char *fmt, ...);

//////////////////////////////////cpp-table begin//////////////////////////////////
namespace cpp_table {

}
//////////////////////////////////cpp-table end//////////////////////////////////

//////////////////////////////////memory-walker begin//////////////////////////////////
namespace memory_walker {

}
//////////////////////////////////memory-walker end//////////////////////////////////

//////////////////////////////////quick-archiver begin//////////////////////////////////
namespace quick_archiver {

class QuickArchiver {
public:
    QuickArchiver();

    ~QuickArchiver();

    int Save(lua_State *L);

    int Load(lua_State *L);

    void SetLzThreshold(size_t size) { m_lz_threshold = size; }

    void SetLzAcceleration(int acceleration) { m_lz_acceleration = acceleration; }

    void SetMaxBufferSize(size_t size);

    static const int MAX_TABLE_DEPTH = 32;

    enum class Type {
        nil,
        number,
        integer,
        string,
        string_idx,
        bool_true,
        bool_false,
        table_hash,
        table_array,
    };
private:
    bool SaveValue(lua_State *L, int idx, int64_t &int_value);

    bool LoadValue(lua_State *L, bool can_be_nil);

private:
    char *m_buffer = 0;
    char *m_lz_buffer = 0;
    std::unordered_map<const char *, int> m_saved_string;
    std::vector<std::pair<const char *, size_t>> m_loaded_string;
    size_t m_buffer_size = 0;
    size_t m_lz_buffer_size = 0;
    size_t m_pos = 0;
    size_t m_table_depth = 0;
    size_t m_lz_threshold = 0;
    int m_lz_acceleration = 1;
};

}
//////////////////////////////////quick-archiver end//////////////////////////////////
