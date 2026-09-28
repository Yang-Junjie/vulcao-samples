add_executable(uniforms main.cpp)

target_link_libraries(uniforms PRIVATE
    vulcao
    samples_common
    glm::glm
)

vulcao_add_slang_shaders(uniforms
    "uniforms.slang|vertMain|vertex|uniforms.vert.spv"
    "uniforms.slang|fragMain|fragment|uniforms.frag.spv"
)
