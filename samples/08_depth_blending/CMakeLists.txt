add_executable(depth_blending main.cpp)
target_link_libraries(depth_blending PRIVATE samples_support)
vulcao_add_slang_shaders(depth_blending
    "layers.slang|vertMain|vertex|layers.vert.spv"
    "layers.slang|fragMain|fragment|layers.frag.spv"
)
