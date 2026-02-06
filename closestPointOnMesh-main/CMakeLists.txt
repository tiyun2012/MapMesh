cmake_minimum_required (VERSION 3.7.1)
project (query)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} Eigen3 include)
file(GLOB SOURCES "src/*.cpp")

add_executable(query ${SOURCES})