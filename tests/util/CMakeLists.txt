add_library(utl_test_util INTERFACE)

target_include_directories(utl_test_util INTERFACE ${PROJECT_SOURCE_DIR}/tests)

target_compile_features(utl_test_util INTERFACE cxx_std_23)
