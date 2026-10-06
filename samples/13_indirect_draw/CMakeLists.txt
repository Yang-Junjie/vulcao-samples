add_executable(indirect_draw main.cpp)
target_link_libraries(indirect_draw PRIVATE samples_support)
vulcao_add_slang_shaders(indirect_draw
    "generate.slang|compMain|compute|generate.comp.spv"
    "draw.slang|vertMain|vertex|draw.vert.spv"
    "draw.slang|fragMain|fragment|draw.frag.spv"
)
