add_library(samples_support INTERFACE)
target_include_directories(samples_support INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(samples_support INTERFACE vulcao)

add_library(samples_common INTERFACE)

target_include_directories(samples_common INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(samples_common INTERFACE
    samples_support
    glfw
    Vulkan::Vulkan
)
