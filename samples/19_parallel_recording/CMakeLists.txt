find_package(Threads REQUIRED)
add_executable(parallel_recording main.cpp)
target_link_libraries(parallel_recording PRIVATE samples_support Threads::Threads)
vulcao_add_slang_shaders(parallel_recording
    "tile.slang|vertMain|vertex|tile.vert.spv"
    "tile.slang|fragMain|fragment|tile.frag.spv"
)
