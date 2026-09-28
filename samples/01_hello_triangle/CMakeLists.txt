add_executable(hello_triangle main.cpp)

target_link_libraries(hello_triangle PRIVATE
    vulcao
    samples_common
)

vulcao_add_slang_shaders(hello_triangle
    "triangle.slang|vertMain|vertex|triangle.vert.spv"
    "triangle.slang|fragMain|fragment|triangle.frag.spv"
)
