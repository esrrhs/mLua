add_executable(test_bin
    test.cpp
)

target_include_directories(test_bin
    PRIVATE
        ${LUA_INCLUDE_DIR}
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(test_bin
    PRIVATE
        ${LUA_LIBRARIES}
)

if(NOT WIN32)
    target_link_libraries(test_bin
        PRIVATE
            ${CMAKE_DL_LIBS}
            m
    )
endif()

# Export Lua symbols so libmluacore.so (loaded via require) can resolve them.
# Do not link mluacore here — requiring the same .so that is also DT_NEEDED
# can double-free on process exit.
if(UNIX AND NOT APPLE)
    target_link_options(test_bin PRIVATE -Wl,--export-dynamic)
elseif(APPLE)
    target_link_options(test_bin PRIVATE -Wl,-export_dynamic)
endif()

target_compile_options(test_bin
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -DNDEBUG -O2 -g3>
)

# Place the shared library at the project root for Lua package.cpath ("../?.so")
add_custom_command(
    TARGET test_bin
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:mluacore>
        ${CMAKE_SOURCE_DIR}/$<TARGET_FILE_NAME:mluacore>
    COMMENT "Copy libmluacore next to project root for Lua package.cpath"
)

add_dependencies(test_bin mluacore)
