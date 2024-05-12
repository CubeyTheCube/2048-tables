cmake_minimum_required(VERSION 3.10)

project(tables)

set(CMAKE_CXX_STANDARD 17)
set(CXXFLAGS  "-O3 -g")
set(CMAKE_CXX_FLAGS "${CXXFLAGS}")


add_executable(tables src/tablegen/table_generator.cpp src/tablegen/board.cpp src/tablegen/interface.cpp src/tablegen/main.cpp)
target_include_directories(tables PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external")
target_include_directories(tables PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/ankerl")


target_link_libraries(tables)
