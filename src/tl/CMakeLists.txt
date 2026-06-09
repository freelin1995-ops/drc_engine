file(GLOB TL_SOURCES ${CMAKE_SOURCE_DIR}/src/tl/tl*.cc)
list(REMOVE_ITEM TL_SOURCES
    ${CMAKE_SOURCE_DIR}/src/tl/tlDeferredExecutionQt.cc
    ${CMAKE_SOURCE_DIR}/src/tl/tlHttpStreamQt.cc
    ${CMAKE_SOURCE_DIR}/src/tl/tlHttpStreamCurl.cc
    ${CMAKE_SOURCE_DIR}/src/tl/tlHttpStreamNoQt.cc
    ${CMAKE_SOURCE_DIR}/src/tl/tlGit.cc
    ${CMAKE_SOURCE_DIR}/src/tl/tlWebDAV.cc
)
add_library(tl STATIC ${TL_SOURCES})
target_include_directories(tl PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/tl)
target_link_libraries(tl PUBLIC ZLIB::ZLIB)
target_compile_definitions(tl PUBLIC MAKE_TL_LIBRARY)
