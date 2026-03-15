add_library(utl_spsc_queue INTERFACE)

target_include_directories(utl_spsc_queue INTERFACE ${UTL_PUBLIC_INCLUDE_ROOT})

target_compile_features(utl_spsc_queue INTERFACE cxx_std_23)

target_link_libraries(utl_spsc_queue
    INTERFACE 
    liballocazam
)
