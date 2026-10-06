add_executable(queries main.cpp)
target_link_libraries(queries PRIVATE samples_support)
vulcao_add_slang_shaders(queries
    "query.slang|vertMain|vertex|query.vert.spv"
    "query.slang|fragMain|fragment|query.frag.spv"
)
