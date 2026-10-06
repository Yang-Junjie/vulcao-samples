add_executable(specialization_cache main.cpp)
target_link_libraries(specialization_cache PRIVATE samples_support)
vulcao_add_slang_shaders(specialization_cache
    "specialize.slang|compMain|compute|specialize.comp.spv"
)
