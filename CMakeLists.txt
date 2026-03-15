cmake_minimum_required(VERSION 3.22)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(LANGS C CXX)

project(utl
    VERSION 0.1.0
    DESCRIPTION "unstandard template library"
    LANGUAGES ${LANGS}
)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(UTL_IS_TOPLEVEL_PROJECT TRUE)
else()
    set(UTL_IS_TOPLEVEL_PROJECT FALSE)
endif()

set(CMAKE_CXX_STANDARD 23)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(UTL_BUILD_TESTS "Build utl test suite" ${UTL_IS_TOPLEVEL_PROJECT})

add_subdirectory(external)
add_subdirectory(lib)

if(UTL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

add_library(utl::libutl ALIAS libutl)
