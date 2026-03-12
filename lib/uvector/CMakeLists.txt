add_library(utl_uvector
    uvector.cpp
)

target_compile_features(utl_uvector PUBLIC cxx_std_23)

target_link_libraries(utl_uvector
    PUBLIC 
    liballocazam
)
