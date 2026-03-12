include(ExternalProject)

find_package(Git)
if(GIT_FOUND)
    message(STATUS "Checking submodules")
    include(check_submodule)
    check_submodule(allocazam)
endif()

# allocazam
add_subdirectory(allocazam)
