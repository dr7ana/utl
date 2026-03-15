add_library(utl_uvector INTERFACE)

target_include_directories(utl_uvector INTERFACE ${UTL_PUBLIC_INCLUDE_ROOT})

target_compile_features(utl_uvector INTERFACE cxx_std_23)

target_link_libraries(utl_uvector
    INTERFACE 
    utl_iterator
    liballocazam
)
