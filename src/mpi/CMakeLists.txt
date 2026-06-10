if(DRC_USE_MPI)
    file(GLOB MPI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
    list(REMOVE_ITEM MPI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cc)
    add_library(mpi STATIC ${MPI_SOURCES})
    target_include_directories(mpi PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/3rd/sol
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(mpi PUBLIC drc rdb gds db tl ${LUA_LIBRARIES} MPI::MPI_CXX)
    target_compile_definitions(mpi PUBLIC DRC_USE_MPI)
endif()
