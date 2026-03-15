# TOOD: 
#   - add target string cmake parameter to select library components to build
#   - create macro to add subdirectory and link to master compilation target

set(UTL_PUBLIC_INCLUDE_ROOT ${PROJECT_BINARY_DIR}/include)
set(UTL_PUBLIC_INCLUDE_PREFIX_ROOT ${UTL_PUBLIC_INCLUDE_ROOT}/utl)

file(MAKE_DIRECTORY ${UTL_PUBLIC_INCLUDE_PREFIX_ROOT})

function(utl_expose_public_headers public_dir source_dir)
    set(public_path ${UTL_PUBLIC_INCLUDE_PREFIX_ROOT}/${public_dir})
    set(source_path ${CMAKE_CURRENT_SOURCE_DIR}/${source_dir})

    if(EXISTS ${public_path} OR IS_SYMLINK ${public_path})
        return()
    endif()

    file(CREATE_LINK ${source_path} ${public_path} SYMBOLIC COPY_ON_ERROR)
endfunction()

utl_expose_public_headers(iterator iterator)
utl_expose_public_headers(spsc spsc)
utl_expose_public_headers(uvector uvector)

add_subdirectory(iterator)
add_subdirectory(spsc)
# add_subdirectory(uvector)

add_library(libutl INTERFACE)

target_link_libraries(libutl INTERFACE utl_iterator utl_spsc_queue)

target_compile_features(libutl INTERFACE cxx_std_23)
