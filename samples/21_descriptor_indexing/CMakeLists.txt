add_executable(descriptor_indexing main.cpp)
target_link_libraries(descriptor_indexing PRIVATE samples_support)
vulcao_add_slang_shaders(descriptor_indexing
    "bindless.slang|compMain|compute|bindless.comp.spv"
)
