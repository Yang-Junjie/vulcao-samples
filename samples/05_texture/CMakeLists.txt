add_executable(texture main.cpp)

target_link_libraries(texture PRIVATE
    vulcao
    samples_common
)

vulcao_add_slang_shaders(texture
    "textured.slang|vertMain|vertex|textured.vert.spv"
    "textured.slang|fragMain|fragment|textured.frag.spv"
)

# The sample decodes a PNG at run time, so the asset directory has to be known
# from wherever the executable is launched.
target_compile_definitions(texture PRIVATE
    "VULCAO_ASSET_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/assets\"")
