add_executable(texel_buffer main.cpp)
target_link_libraries(texel_buffer PRIVATE samples_support)
vulcao_add_slang_shaders(texel_buffer
    "texels.slang|compMain|compute|texels.comp.spv"
)
