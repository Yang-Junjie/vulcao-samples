add_executable(compute main.cpp)

# No samples_common: a compute pass needs no window, so this sample stays headless.
target_link_libraries(compute PRIVATE vulcao)

vulcao_add_slang_shaders(compute
    "transform.slang|compMain|compute|transform.comp.spv"
)
