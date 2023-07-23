#include "core.h"

int main(int argc, char *argv[]) {
    auto L = luaL_newstate();
    luaL_openlibs(L);

    if (luaL_dofile(L, "./test/test.lua") != 0) {
        printf("test luaL_dofile failed %s\n", lua_tostring(L, -1));
        return -1;
    }

    return 0;
}
