cmake_minimum_required(VERSION 2.8)
project(test)

include_directories(.)
include_directories(../)

aux_source_directory(./ TEST_SRC_LIST)

add_executable(test_bin ${TEST_SRC_LIST})
IF (WIN32)
    target_link_libraries(test_bin lua mluacore)
ELSE ()
    target_link_libraries(test_bin lua mluacore dl)
ENDIF ()
