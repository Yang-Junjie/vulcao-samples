add_executable(storage_image main.cpp)
target_link_libraries(storage_image PRIVATE samples_support)
vulcao_add_slang_shaders(storage_image
    "generate.slang|compMain|compute|generate.comp.spv"
    "display.slang|vertMain|vertex|display.vert.spv"
    "display.slang|fragMain|fragment|display.frag.spv"
)
