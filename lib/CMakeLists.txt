# TOOD: 
#   - add target string cmake parameter to select library components to build
#   - create macro to add subdirectory and link to master compilation target

add_subdirectory(common)
add_subdirectory(spsc)
add_subdirectory(uvector)

add_library(libutl INTERFACE)

target_link_libraries(libutl INTERFACE utl_common utl_uvector utl_spsc_queue)

target_compile_features(libutl INTERFACE cxx_std_23)

