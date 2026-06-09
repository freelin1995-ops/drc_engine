file(GLOB DB_SOURCES ${CMAKE_SOURCE_DIR}/src/db/db*.cc)
list(REMOVE_ITEM DB_SOURCES
    # Exclude LVS/netlist/circuit files (not needed for flat DRC)
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlist.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistCompare.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistCompareCore.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistCompareGraph.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistCompareUtils.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistCrossReference.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistDeviceClasses.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistDeviceExtractor.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistDeviceExtractorClasses.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistExtractor.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistReader.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistSpiceReader.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistSpiceReaderDelegate.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistSpiceReaderExpressionParser.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistSpiceWriter.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistUtils.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetlistWriter.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbNetShape.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbCircuit.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbLayoutToNetlist.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbLayoutToNetlistFormatDefs.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbLayoutToNetlistReader.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbLayoutToNetlistSoftConnections.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbLayoutToNetlistWriter.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbLayoutVsSchematic.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbLayoutVsSchematicFormatDefs.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbLayoutVsSchematicReader.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbLayoutVsSchematicWriter.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbLayoutContextHandler.cc
    # Exclude deep shape store (references LayoutToNetlist status_changed_event)
    ${CMAKE_SOURCE_DIR}/src/db/dbDeepShapeStore.cc
    # Exclude GUI-related
    ${CMAKE_SOURCE_DIR}/src/db/dbGlyphs.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbHershey.cc
    # Exclude tiling (not needed for flat mode initially)
    ${CMAKE_SOURCE_DIR}/src/db/dbTilingProcessor.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbFillTool.cc
    # Exclude hierarchical processing (shape_interactions templates are in dbHierProcessor.cc)
    ${CMAKE_SOURCE_DIR}/src/db/dbHierNetworkProcessor.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbHierarchyBuilder.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbHierProcessorUtils.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbCellHullGenerator.cc
    ${CMAKE_SOURCE_DIR}/src/db/dbCellInstanceSetHasher.cc
)
add_library(db STATIC ${DB_SOURCES})
target_include_directories(db PUBLIC ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/include/db)
target_include_directories(db PUBLIC ${CMAKE_SOURCE_DIR}/include/db/gsi_stubs)
target_link_libraries(db PUBLIC tl)
target_compile_definitions(db PUBLIC MAKE_DB_LIBRARY)
