add_executable(dynamic_uniforms main.cpp)
target_link_libraries(dynamic_uniforms PRIVATE samples_support)
vulcao_add_slang_shaders(dynamic_uniforms
    "objects.slang|vertMain|vertex|objects.vert.spv"
    "objects.slang|fragMain|fragment|objects.frag.spv"
)
