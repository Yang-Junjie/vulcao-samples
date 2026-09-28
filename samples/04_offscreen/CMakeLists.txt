add_executable(offscreen main.cpp)

# No samples_common: rendering into an image needs no window.
target_link_libraries(offscreen PRIVATE vulcao)

vulcao_add_slang_shaders(offscreen
    "triangle.slang|vertMain|vertex|triangle.vert.spv"
    "triangle.slang|fragMain|fragment|triangle.frag.spv"
)
