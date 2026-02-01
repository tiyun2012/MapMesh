#-
# ==========================================================================
# MetaHuman-to-Custom Mesh Fitter (MatchMesh)
# ==========================================================================

cmake_minimum_required(VERSION 3.13)

# include the project setting file
include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/pluginEntry.cmake)

set(PROJECT_NAME MatchMesh)

# Optional: flip to ON to use TBB parallel_for (shipped with devkitBase2026)
option(MATCHMESH_USE_TBB "Use Intel TBB parallel loops" ON)

set(SOURCE_FILES
    pluginMain.cpp
    PinLocatorNode.cpp
    PinLocatorNode.h
    MatchMeshCreatePinCmd.cpp
    MatchMeshCreatePinCmd.h
    DualViewportUICmd.cpp
    DualViewportUICmd.h
)

# Avoid linker contention on existing binaries by writing to a local bin dir
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
foreach(cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    string(TOUPPER ${cfg} CFGU)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFGU} ${CMAKE_CURRENT_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFGU} ${CMAKE_CURRENT_BINARY_DIR}/bin)
endforeach()

# Link against Maya core libs
set(LIBRARIES
    OpenMaya
    OpenMayaAnim
    OpenMayaRender
    OpenMayaUI
    Foundation
)

if(MATCHMESH_USE_TBB)
    add_definitions(-DMATCHMESH_USE_TBB)
    find_tbb()
endif()

build_plugin()
