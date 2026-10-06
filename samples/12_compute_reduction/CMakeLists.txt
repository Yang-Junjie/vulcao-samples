add_executable(compute_reduction main.cpp)
target_link_libraries(compute_reduction PRIVATE samples_support)
vulcao_add_slang_shaders(compute_reduction
    "reduce.slang|compMain|compute|reduce.comp.spv"
)
