add_library(utl_common INTERFACE)

target_include_directories(utl_common INTERFACE ${PROJECT_SOURCE_DIR}/lib/common)

target_compile_features(utl_common INTERFACE cxx_std_23)
