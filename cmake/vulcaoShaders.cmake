# Helpers for compiling Slang shaders into SPIR-V.
#
# The samples treat slangc as required; it ships with the Vulkan SDK.

# Sets <out_var> to the slangc executable, or to "<out_var>-NOTFOUND".
function(vulcao_find_slangc out_var)
    get_filename_component(vulkan_bin_dir "${Vulkan_GLSLC_EXECUTABLE}" DIRECTORY)
    find_program(VULCAO_SLANGC_EXECUTABLE NAMES slangc HINTS "${vulkan_bin_dir}")
    set(${out_var} "${VULCAO_SLANGC_EXECUTABLE}" PARENT_SCOPE)
endfunction()

# Compiles Slang shaders for a target and defines VULCAO_SHADER_DIR to the
# directory holding the SPIR-V output.
#
# Sources are read from "<caller>/shaders" and written to
# "<caller binary dir>/shaders". Each shader is "<source>|<entry>|<stage>|<output>".
function(vulcao_add_slang_shaders target)
    vulcao_find_slangc(SLANGC_EXECUTABLE)
    if(NOT SLANGC_EXECUTABLE)
        message(FATAL_ERROR "slangc not found; install the Vulkan SDK")
    endif()

    set(shader_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    set(shader_output_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${shader_output_dir}")

    set(outputs)
    foreach(shader IN LISTS ARGN)
        string(REPLACE "|" ";" fields "${shader}")
        list(GET fields 0 source)
        list(GET fields 1 entry)
        list(GET fields 2 stage)
        list(GET fields 3 output)

        add_custom_command(
            OUTPUT "${shader_output_dir}/${output}"
            COMMAND "${SLANGC_EXECUTABLE}" "${shader_source_dir}/${source}"
                    -target spirv -entry "${entry}" -stage "${stage}"
                    -fvk-use-entrypoint-name
                    -o "${shader_output_dir}/${output}"
            DEPENDS "${shader_source_dir}/${source}"
            COMMENT "Compiling ${source} (${entry})"
            VERBATIM
        )
        list(APPEND outputs "${shader_output_dir}/${output}")
    endforeach()

    add_custom_target(${target}_shaders DEPENDS ${outputs})
    add_dependencies(${target} ${target}_shaders)

    target_compile_definitions(${target} PRIVATE
        "VULCAO_SHADER_DIR=\"${shader_output_dir}\"")
endfunction()
