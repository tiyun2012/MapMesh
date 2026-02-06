#-
# ==========================================================================
# Neighboring Vertices Node
# ==========================================================================

cmake_minimum_required(VERSION 3.13)
project(neighboringVertices)

# include the project setting file
include($ENV{DEVKIT_LOCATION}/cmake/pluginEntry.cmake)

# specify project name
set(PROJECT_NAME neighboringVertices)

# set SOURCE_FILES
set(SOURCE_FILES
   neighboringvertices_node.cpp
)

# set linking libraries
set(LIBRARIES
     OpenMaya
     Foundation
)

# Build plugin
build_plugin()
