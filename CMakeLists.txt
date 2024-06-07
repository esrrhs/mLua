cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
project(mlua)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++17" SUPPORT_CXX17)
if (SUPPORT_CXX17)
    set(CMAKE_CXX_STANDARD 17)
    message(STATUS "C++17 supported")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/roaring4)
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/roaring4 MLUA_SRC_LIST)
else()
    set(CMAKE_CXX_STANDARD 11)
    message(STATUS "C++17 not supported, use C++11 instead")
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/roaring2)
    aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/roaring2 MLUA_SRC_LIST)
    add_definitions(-DROARING_EXCEPTIONS=OFF -DCROARING_ATOMIC_IMPL=1 -DROARING_DISABLE_X64)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/)

IF (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -DNDEBUG -O0 -g3 -ggdb -fPIC")
ELSE ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -DNDEBUG -O2 -g3 -ggdb -fPIC")
ENDIF ()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -DNDEBUG -O2 -g3 -ggdb -fPIC")

include_directories(.)
include_directories(./thirdparty)

aux_source_directory(./ MLUA_SRC_LIST)

add_library(mluacore SHARED ${MLUA_SRC_LIST})
IF (WIN32)
    target_link_libraries(mluacore lua)
ELSE ()
    target_link_libraries(mluacore lua dl)
ENDIF ()

add_subdirectory(test)
