file(GLOB RDB_SOURCES ${CMAKE_SOURCE_DIR}/src/rdb/*.cc)
list(REMOVE_ITEM RDB_SOURCES
    ${CMAKE_SOURCE_DIR}/src/rdb/rdbTiledRdbOutputReceiver.cc
    ${CMAKE_SOURCE_DIR}/src/rdb/rdbRVEReader.cc
)
add_library(rdb STATIC ${RDB_SOURCES})
target_include_directories(rdb PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/rdb)
target_link_libraries(rdb PUBLIC db tl)
target_compile_definitions(rdb PUBLIC MAKE_RDB_LIBRARY)
