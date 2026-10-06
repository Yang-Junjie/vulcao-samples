add_executable(msaa main.cpp)
target_link_libraries(msaa PRIVATE samples_support)
vulcao_add_slang_shaders(msaa
    "triangle.slang|vertMain|vertex|triangle.vert.spv"
    "triangle.slang|fragMain|fragment|triangle.frag.spv"
)
