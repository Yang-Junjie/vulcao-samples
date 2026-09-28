include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/vulcaoShaders.cmake")

add_subdirectory(common)

add_subdirectory(01_hello_triangle)
add_subdirectory(02_uniforms)
add_subdirectory(03_compute)
add_subdirectory(04_offscreen)
add_subdirectory(05_texture)
add_subdirectory(06_two_pass)
