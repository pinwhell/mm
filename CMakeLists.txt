cmake_minimum_required(VERSION 3.10)

project(mm)

option(MM_FREESTANDING "Use freestanding mode (disable <string.h>)" OFF)
set(ARENA_ARR_SZ 16 CACHE STRING "Number of arenas")

add_library(mm STATIC src/mm.c)
target_include_directories(mm PUBLIC include)
# Define macros for the compiler
if(MM_FREESTANDING)
    target_compile_definitions(mm PUBLIC MM_FREESTANDING=1)
endif()

target_compile_definitions(mm PUBLIC ARENA_ARR_SZ=${ARENA_ARR_SZ})

add_subdirectory(tests)