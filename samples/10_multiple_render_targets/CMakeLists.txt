add_executable(multiple_render_targets main.cpp)
target_link_libraries(multiple_render_targets PRIVATE samples_support)
vulcao_add_slang_shaders(multiple_render_targets
    "gbuffer.slang|vertMain|vertex|gbuffer.vert.spv"
    "gbuffer.slang|fragMain|fragment|gbuffer.frag.spv"
)
