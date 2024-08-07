#include "core.h"
#include "cpp_table.h"
#include "memory_walker.h"
#include "quick_archiver.h"

void llog(int level, const char *file, const char *func, int pos, const char *fmt, ...) {
    auto now = time(nullptr);
    auto ms = (int) std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count() % 1000;
    auto tm = localtime(&now);

    // print log to stdout or stderr, and use color to distinguish different log level
    // 2020-01-01 00:00:00.000 [DEBUG] core.cpp:main:123: hello world
    // 2020-01-01 00:00:00.000 [ERROR] core.cpp:main:123: hello world
#ifdef _WIN32
    // if in windows, no color
    va_list args;
    va_start(args, fmt);
    printf("%04d-%02d-%02d %02d:%02d:%02d.%03d[%s] %s:%s:%d: ", tm->tm_year + 1900, tm->tm_mon + 1,
           tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, ms, level == LOG_DEBUG_LEVEL ? "DEBUG" : "ERROR",
           file, func, pos);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
#else
    auto color = level == LOG_DEBUG_LEVEL ? "\033[32m" : "\033[31m";
    va_list args;
    va_start(args, fmt);
    printf("\033[0m%04d-%02d-%02d %02d:%02d:%02d.%03d %s[%s] %s:%s:%d: ", tm->tm_year + 1900, tm->tm_mon + 1,
           tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, ms, color, level == LOG_DEBUG_LEVEL ? "DEBUG" : "ERROR",
           file, func, pos);
    vprintf(fmt, args);
    printf("\033[0m\n");
    va_end(args);
#endif
}

extern "C" int luaopen_libmluacore(lua_State *L) {
    LLOG("luaopen_libmluacore start");
    std::vector<luaL_Reg> l;
    auto cpp_table_funcs = GetCppTableFuncs();
    l.insert(l.end(), cpp_table_funcs.begin(), cpp_table_funcs.end());
    auto memory_walker_funcs = GetMemoryWalkerFuncs();
    l.insert(l.end(), memory_walker_funcs.begin(), memory_walker_funcs.end());
    auto quick_archiver_funcs = GetQuickArchiverFuncs();
    l.insert(l.end(), quick_archiver_funcs.begin(), quick_archiver_funcs.end());
    l.push_back({nullptr, nullptr});
    lua_createtable(L, 0, l.size() - 1);
    luaL_setfuncs(L, l.data(), 0);
    LLOG("luaopen_libmluacore success function count %d", (int) l.size() - 1);
    return 1;
}
