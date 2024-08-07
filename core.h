#pragma once

#include <string>
#include <list>
#include <vector>
#include <map>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <chrono>
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

// for debug
#define LOG_DEBUG_LEVEL 0
#define LOG_ERROR_LEVEL 1
#define LOG_LEVEL LOG_ERROR_LEVEL

#define LLOG(...) if (LOG_LEVEL <= LOG_DEBUG_LEVEL) llog(LOG_DEBUG_LEVEL, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LERR(...) if (LOG_LEVEL <= LOG_ERROR_LEVEL) llog(LOG_ERROR_LEVEL, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

void llog(int level, const char *file, const char *func, int pos, const char *fmt, ...);
