add_executable(drc-check main.cc)
target_include_directories(drc-check PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/3rd/sol)
# Use --whole-archive for gds to ensure format registrations (GDS2, etc.) are included
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_link_libraries(drc-check drc rdb -Wl,--whole-archive gds -Wl,--no-whole-archive db tl ${LUA_LIBRARIES})
    target_link_options(drc-check PRIVATE -Wl,--unresolved-symbols=ignore-in-object-files)
else()
    target_link_libraries(drc-check drc rdb gds db tl ${LUA_LIBRARIES})
endif()
