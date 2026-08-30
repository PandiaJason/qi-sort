cmake_minimum_required(VERSION 3.14)
project(qi_sort VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Interface library target for header-only distribution
add_library(qi_sort INTERFACE)
target_include_directories(qi_sort INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optional Example Targets
option(BUILD_QI_EXAMPLES "Build example applications" ON)

if(BUILD_QI_EXAMPLES)
    add_executable(basic_usage examples/basic_usage.cpp)
    target_link_libraries(basic_usage PRIVATE qi_sort)

    add_executable(database_column_sort examples/database_column_sort.cpp)
    target_link_libraries(database_column_sort PRIVATE qi_sort)

    add_executable(spatial_morton_sort examples/spatial_morton_sort.cpp)
    target_link_libraries(spatial_morton_sort PRIVATE qi_sort)

    add_executable(boost_sort_example examples/boost_sort_example.cpp)
    target_link_libraries(boost_sort_example PRIVATE qi_sort)
endif()
