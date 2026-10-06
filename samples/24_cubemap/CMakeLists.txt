add_executable(cubemap main.cpp)
target_link_libraries(cubemap PRIVATE samples_support)
vulcao_add_slang_shaders(cubemap
    "faces.slang|compMain|compute|faces.comp.spv"
)
