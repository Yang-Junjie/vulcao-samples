add_executable(deferred_destruction main.cpp)
target_link_libraries(deferred_destruction PRIVATE samples_common)
vulcao_add_slang_shaders(deferred_destruction
    "triangle.slang|vertMain|vertex|triangle.vert.spv"
    "triangle.slang|fragMain|fragment|triangle.frag.spv"
)
