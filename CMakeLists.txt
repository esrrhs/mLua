cmake_minimum_required(VERSION 3.12)
project(mlua LANGUAGES C CXX)

include(CheckCXXCompilerFlag)
include(GNUInstallDirs)

check_cxx_compiler_flag("-std=c++17" SUPPORT_CXX17)
if(SUPPORT_CXX17)
    set(CMAKE_CXX_STANDARD 17)
    message(STATUS "C++17 supported")
    set(MLUA_ROARING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/roaring3)
else()
    set(CMAKE_CXX_STANDARD 11)
    message(STATUS "C++17 not supported, use C++11 instead")
    set(MLUA_ROARING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/roaring2)
    add_compile_definitions(
        ROARING_EXCEPTIONS=OFF
        CROARING_ATOMIC_IMPL=1
        ROARING_DISABLE_X64
    )
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
# CRoaring needs C11 atomics when compiled as C
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Keep build artifacts together so the test binary can load libmluacore.so
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Prefer a shared Lua library so it can be linked into the test binary / modules
if(NOT WIN32)
    set(_mlua_saved_suffixes "${CMAKE_FIND_LIBRARY_SUFFIXES}")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".so" ".dylib" ".a")
endif()

find_package(Lua REQUIRED)

if(NOT WIN32)
    set(CMAKE_FIND_LIBRARY_SUFFIXES "${_mlua_saved_suffixes}")
endif()

message(STATUS "Lua include dir: ${LUA_INCLUDE_DIR}")
message(STATUS "Lua libraries:   ${LUA_LIBRARIES}")

set(MLUA_CORE_SOURCES
    core.cpp
    cpp_table.cpp
    memory_walker.cpp
    quick_archiver.cpp
    lz4.c
    ${MLUA_ROARING_DIR}/roaring.c
)

add_library(mluacore SHARED ${MLUA_CORE_SOURCES})

target_include_directories(mluacore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty
        ${MLUA_ROARING_DIR}
        ${LUA_INCLUDE_DIR}
)

set_target_properties(mluacore PROPERTIES
    OUTPUT_NAME "mluacore"
    PREFIX "lib"
    POSITION_INDEPENDENT_CODE ON
)

target_compile_options(mluacore
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -DNDEBUG -O2 -g3>
)

# Lua C modules leave Lua symbols unresolved on UNIX; they are provided by the
# host interpreter (or by the embedding executable linked with --export-dynamic).
if(WIN32)
    target_link_libraries(mluacore
        PUBLIC
            ${LUA_LIBRARIES}
    )
elseif(APPLE)
    target_link_options(mluacore PRIVATE -undefined dynamic_lookup)
else()
    target_link_libraries(mluacore
        PRIVATE
            ${CMAKE_DL_LIBS}
            m
    )
endif()

add_subdirectory(test)

enable_testing()

# Prefer the Lua interpreter so libmluacore.so is loaded once via require()
# (linking it into test_bin and also requiring it can double-free on exit).
find_program(LUA_EXECUTABLE NAMES lua lua5.4 lua5.3 lua5.2 lua5.1)
if(LUA_EXECUTABLE)
    add_test(
        NAME quick_archiver_test
        COMMAND ${CMAKE_COMMAND} -E env
            "LUA_CPATH=${CMAKE_SOURCE_DIR}/?.so;${CMAKE_BINARY_DIR}/bin/?.so;;"
            "LUA_PATH=${CMAKE_SOURCE_DIR}/?.lua;${CMAKE_SOURCE_DIR}/test/?.lua;;"
            ${LUA_EXECUTABLE} ${CMAKE_SOURCE_DIR}/test/test_quick_archiver.lua
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
    )
    add_test(
        NAME static_perf_test
        COMMAND ${CMAKE_COMMAND} -E env
            "LUA_CPATH=${CMAKE_SOURCE_DIR}/?.so;${CMAKE_BINARY_DIR}/bin/?.so;;"
            "LUA_PATH=${CMAKE_SOURCE_DIR}/?.lua;${CMAKE_SOURCE_DIR}/test/?.lua;;"
            ${LUA_EXECUTABLE} ${CMAKE_SOURCE_DIR}/test/test_static_perf.lua
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
    )
else()
    message(WARNING "Lua interpreter not found; falling back to test_bin for tests")
    add_test(
        NAME quick_archiver_test
        COMMAND $<TARGET_FILE:test_bin> ${CMAKE_SOURCE_DIR}/test/test_quick_archiver.lua
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
    )
    add_test(
        NAME static_perf_test
        COMMAND $<TARGET_FILE:test_bin> ${CMAKE_SOURCE_DIR}/test/test_static_perf.lua
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
    )
endif()

# Optionally build Go tools if Go is available (same pattern as pLua)
find_program(GO_EXECUTABLE go)
if(GO_EXECUTABLE)
    add_custom_target(tools_mlua ALL
        COMMAND ${GO_EXECUTABLE} build -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plua plua.go
        COMMAND ${GO_EXECUTABLE} build -o ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/proto proto.go
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tools
        COMMENT "Building tools/plua and tools/proto via Go"
    )
endif()
