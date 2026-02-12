cmake_minimum_required(VERSION 3.13)
project(tcHarmonicDeformer)

# include the project setting file
include($ENV{DEVKIT_LOCATION}/cmake/pluginEntry.cmake)

# specify project name
set(PROJECT_NAME tcHarmonicDeformer)

# set SOURCE_FILES
set(SOURCE_FILES
    cell.cpp
    computeWeightsCmd.cpp
    grid.cpp
    harmonicDeformer.cpp
    harmonicDeformerCmd.cpp
    intersect.cpp
    mathUtils.cpp
    pluginMain.cpp
    solver.cpp
)

# set linking libraries
set(LIBRARIES
    OpenMaya
    OpenMayaAnim
    Foundation
)

find_tbb()

# Build plugin
build_plugin()
