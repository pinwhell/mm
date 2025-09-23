cmake_minimum_required(VERSION 3.10)

project(mm)

option(MM_FREESTANDING "Use freestanding mode (disable <string.h>)" OFF)
option(MM_MT "Use multi-thread" ON)
set(ARENA_ARR_SZ 16 CACHE STRING "Number of arenas")

add_subdirectory(vendor)

add_library(mm INTERFACE)
target_include_directories(mm INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Define macros for the compiler
if(MM_FREESTANDING)
    target_compile_definitions(mm INTERFACE MM_FREESTANDING=1)
endif()

if(MM_MT AND NOT MM_FREESTANDING)
    target_link_libraries(mm INTERFACE spnlck)
    target_compile_definitions(mm INTERFACE MM_MT=1)
endif()


target_compile_definitions(mm INTERFACE ARENA_ARR_SZ=${ARENA_ARR_SZ})

add_subdirectory(tests)