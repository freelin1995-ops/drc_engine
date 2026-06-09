file(GLOB DRC_SOURCES ${CMAKE_SOURCE_DIR}/src/drc/*.cc)
add_library(drc STATIC ${DRC_SOURCES})
target_include_directories(drc PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/3rd/sol
)
target_link_libraries(drc PUBLIC db rdb gds tl ${LUA_LIBRARIES})
target_compile_definitions(drc PUBLIC MAKE_DRC_LIBRARY)
