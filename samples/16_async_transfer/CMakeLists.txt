add_executable(async_transfer main.cpp)
target_link_libraries(async_transfer PRIVATE samples_support)
vulcao_add_slang_shaders(async_transfer
    "consume.slang|compMain|compute|consume.comp.spv"
)
