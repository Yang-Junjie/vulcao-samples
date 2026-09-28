add_executable(two_pass main.cpp)

target_link_libraries(two_pass PRIVATE
    vulcao
    samples_common
    glm::glm
)

vulcao_add_slang_shaders(two_pass
    "scene.slang|vertMain|vertex|scene.vert.spv"
    "scene.slang|fragMain|fragment|scene.frag.spv"
    "post.slang|vertMain|vertex|post.vert.spv"
    "post.slang|fragMain|fragment|post.frag.spv"
)

# The sample loads the teapot at run time, so the asset directory has to be
# known from wherever the executable is launched.
target_compile_definitions(two_pass PRIVATE
    "VULCAO_ASSET_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\"")
