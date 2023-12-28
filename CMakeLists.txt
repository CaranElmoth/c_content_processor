cmake_minimum_required(VERSION 3.0.0)
project(c_content_processor VERSION 0.1.0)

include(CTest)
enable_testing()

include_directories(./include)
add_executable(c_content_processor src/main.c)
target_sources(c_content_processor PRIVATE src/cJSON.c)
target_sources(c_content_processor PRIVATE src/processors/ldtk_to_map.c)
target_sources(c_content_processor PRIVATE src/processors/png_to_png.c)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
