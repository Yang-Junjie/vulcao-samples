add_library(samples_common INTERFACE)

target_include_directories(samples_common INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(samples_common INTERFACE
    glfw
    Vulkan::Vulkan
)
