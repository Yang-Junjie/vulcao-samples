include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/vulcaoShaders.cmake")

add_subdirectory(common)

add_subdirectory(01_hello_triangle)
add_subdirectory(02_uniforms)
add_subdirectory(03_compute)
add_subdirectory(04_offscreen)
add_subdirectory(05_texture)
add_subdirectory(06_two_pass)
add_subdirectory(07_instancing)
add_subdirectory(08_depth_blending)
add_subdirectory(09_msaa)
add_subdirectory(10_multiple_render_targets)
add_subdirectory(11_storage_image)
add_subdirectory(12_compute_reduction)
add_subdirectory(13_indirect_draw)
add_subdirectory(14_dynamic_uniforms)
add_subdirectory(15_texture_array)
add_subdirectory(16_async_transfer)
add_subdirectory(17_queries)
add_subdirectory(18_specialization_cache)
add_subdirectory(19_parallel_recording)
add_subdirectory(20_transfer_regions)
add_subdirectory(21_descriptor_indexing)
add_subdirectory(22_deferred_destruction)
add_subdirectory(23_texel_buffer)
add_subdirectory(24_cubemap)

if(BUILD_TESTING)
    foreach(target IN ITEMS compute offscreen depth_blending msaa
                            multiple_render_targets storage_image compute_reduction
                            indirect_draw dynamic_uniforms texture_array async_transfer
                            queries specialization_cache parallel_recording transfer_regions
                            descriptor_indexing texel_buffer cubemap)
        add_test(NAME sample_${target} COMMAND $<TARGET_FILE:${target}>)
        set_tests_properties(sample_${target} PROPERTIES
            LABELS "headless" TIMEOUT 60 SKIP_RETURN_CODE 77)
    endforeach()
    add_test(NAME sample_async_transfer_dedicated COMMAND $<TARGET_FILE:async_transfer> --dedicated)
    set_tests_properties(sample_async_transfer_dedicated PROPERTIES
        LABELS "headless;optional" TIMEOUT 60 SKIP_RETURN_CODE 77)
    if(VULCAO_SAMPLES_WINDOW_TESTS)
        add_test(NAME sample_instancing COMMAND $<TARGET_FILE:instancing> --frames 6)
        set_tests_properties(sample_instancing PROPERTIES LABELS "window" TIMEOUT 60)
        add_test(NAME sample_deferred_destruction COMMAND $<TARGET_FILE:deferred_destruction> --frames 9)
        set_tests_properties(sample_deferred_destruction PROPERTIES LABELS "window" TIMEOUT 60)
    endif()
endif()
