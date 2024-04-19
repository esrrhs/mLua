#include "core.h"
#include <chrono>

static int get_cur_time_ms(lua_State *L) {
    auto ret = std::chrono::high_resolution_clock::now().time_since_epoch().count() / 1000 / 1000;
    lua_pushinteger(L, ret);
    return 1;
}

int main(int argc, char *argv[]) {
    auto L = luaL_newstate();
    luaL_openlibs(L);

    lua_pushcfunction(L, get_cur_time_ms);
    lua_setglobal(L, "get_cur_time_ms");

    if (luaL_dofile(L, "./test/test_perf.lua") != 0) {
        printf("test luaL_dofile failed %s\n", lua_tostring(L, -1));
        return -1;
    }

    return 0;
}
