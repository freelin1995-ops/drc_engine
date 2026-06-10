add_executable(test_engine test_engine.cc)
target_include_directories(test_engine PRIVATE ${CMAKE_SOURCE_DIR}/include)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_link_libraries(test_engine drc rdb -Wl,--whole-archive gds -Wl,--no-whole-archive db tl)
    target_link_options(test_engine PRIVATE -Wl,--unresolved-symbols=ignore-in-object-files)
else()
    target_link_libraries(test_engine drc rdb gds db tl)
endif()
add_test(NAME engine COMMAND test_engine)

add_executable(test_mpi_serialize test_mpi_serialize.cc ../src/mpi/mpi_serialize.cc)
target_include_directories(test_mpi_serialize PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_link_libraries(test_mpi_serialize drc rdb -Wl,--whole-archive gds -Wl,--no-whole-archive db tl)
    target_link_options(test_mpi_serialize PRIVATE -Wl,--unresolved-symbols=ignore-in-object-files)
else()
    target_link_libraries(test_mpi_serialize drc rdb gds db tl)
endif()
add_test(NAME mpi_serialize COMMAND test_mpi_serialize)
