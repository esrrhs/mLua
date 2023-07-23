cmake_minimum_required(VERSION 2.8)
project(mlua)

set(CMAKE_CXX_STANDARD 11)

include(GNUInstallDirs)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/)

include_directories(.)

aux_source_directory(./ MLUA_SRC_LIST)

add_library(mluacore SHARED ${MLUA_SRC_LIST})
IF (WIN32)
    target_link_libraries(mluacore lua)
ELSE ()
    target_link_libraries(mluacore lua dl)
ENDIF ()

add_subdirectory(test)
