add_executable(texture_array main.cpp)
target_link_libraries(texture_array PRIVATE samples_support)
vulcao_add_slang_shaders(texture_array
    "array.slang|vertMain|vertex|array.vert.spv"
    "array.slang|fragMain|fragment|array.frag.spv"
)
