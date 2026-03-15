add_library(utl_iterator INTERFACE)

target_include_directories(utl_iterator INTERFACE ${UTL_PUBLIC_INCLUDE_ROOT})

target_compile_features(utl_iterator INTERFACE cxx_std_23)
