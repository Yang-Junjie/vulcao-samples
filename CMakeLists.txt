cmake_minimum_required(VERSION 3.24)
project(vulcao-samples VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)

# --- vulcao (git submodule, built from source as a subproject) ---------------

# Only the library is wanted here; vulcao's own tests are not built.
set(VULCAO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(VULCAO_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/vulcao "${CMAKE_BINARY_DIR}/vulcao")

# --- windowing and math (samples only) ---------------------------------------

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glfw)

set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glm)

# --- samples -----------------------------------------------------------------

add_subdirectory(samples)
