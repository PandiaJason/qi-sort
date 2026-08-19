cmake_minimum_required(VERSION 3.14)
project(qi_radix VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Interface library target for header-only distribution
add_library(qi_radix INTERFACE)
target_include_directories(qi_radix INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optional Example Targets
option(BUILD_QI_EXAMPLES "Build QI-Radix example applications" ON)

if(BUILD_QI_EXAMPLES)
    add_executable(basic_usage examples/basic_usage.cpp)
    target_link_libraries(basic_usage PRIVATE qi_radix)

    add_executable(analytical_inspection examples/analytical_inspection.cpp)
    target_link_libraries(analytical_inspection PRIVATE qi_radix)

    add_executable(database_column_sort examples/database_column_sort.cpp)
    target_link_libraries(database_column_sort PRIVATE qi_radix)

    add_executable(spatial_morton_sort examples/spatial_morton_sort.cpp)
    target_link_libraries(spatial_morton_sort PRIVATE qi_radix)
endif()
