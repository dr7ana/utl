add_executable(utl_spsc_queue_core_behavior queue_core_behavior.cpp)

target_link_libraries(utl_spsc_queue_core_behavior
    PRIVATE
    utl_spsc_queue
    utl_test_util
)

set_target_properties(utl_spsc_queue_core_behavior PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/spsc)

add_test(NAME utl_spsc_queue_core_behavior COMMAND utl_spsc_queue_core_behavior)
