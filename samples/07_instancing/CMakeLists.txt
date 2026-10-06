add_executable(instancing main.cpp)
target_link_libraries(instancing PRIVATE samples_common)
vulcao_add_slang_shaders(instancing
    "instances.slang|vertMain|vertex|instances.vert.spv"
    "instances.slang|fragMain|fragment|instances.frag.spv"
)
