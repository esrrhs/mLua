#include "core.h"
#include "lz4.h"

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

std::vector<luaL_Reg> GetQuickArchiverFuncs();
