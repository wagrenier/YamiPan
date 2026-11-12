# Compiles resources/shaders/hlsl/*.hlsl to the three bytecodes SDL_GPU backends consume, with the
# SDL_shadercross CLI, and deploys them next to the executable. The CLI is not built here (its
# DirectXShaderCompiler does not compile under MinGW): a prebuilt one comes from extern/ or a sibling
# checkout, or -DSHADERCROSS_EXECUTABLE=<path>. Without one, the bytecode in the tree is used as it is.
set(YAMIPAN_HLSL_DIR ${CMAKE_SOURCE_DIR}/resources/shaders/hlsl)
file(GLOB YAMIPAN_HLSL_SHADERS CONFIGURE_DEPENDS ${YAMIPAN_HLSL_DIR}/*.hlsl)
file(GLOB YAMIPAN_HLSL_INCLUDES CONFIGURE_DEPENDS ${YAMIPAN_HLSL_DIR}/*.hlsli)

find_program(SHADERCROSS_EXECUTABLE NAMES shadercross
    HINTS ${CMAKE_SOURCE_DIR}/extern/shadercross/windows ${YAMIPAN_SIBLING_EXTERN}/shadercross/windows
    DOC "The SDL_shadercross CLI")

if(NOT SHADERCROSS_EXECUTABLE)
    message(WARNING "shadercross not found: resources/shaders/{spirv,dxil,msl} are used as they are.")
    add_custom_target(compile_shaders)
    return()
endif()
message(STATUS "shadercross: ${SHADERCROSS_EXECUTABLE}")

set(YAMIPAN_SHADER_OUTPUTS)
foreach(format "spirv=spv" "dxil=dxil" "msl=msl")
    string(REPLACE "=" ";" parts ${format})
    list(GET parts 0 directory)
    list(GET parts 1 extension)
    set(out_dir ${CMAKE_SOURCE_DIR}/resources/shaders/${directory})
    set(run_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/${directory})
    file(MAKE_DIRECTORY ${out_dir})
    foreach(hlsl ${YAMIPAN_HLSL_SHADERS})
        get_filename_component(name ${hlsl} NAME)
        string(REGEX REPLACE "\\.hlsl$" ".${extension}" out_name ${name})
        add_custom_command(
            OUTPUT ${out_dir}/${out_name}
            COMMAND ${SHADERCROSS_EXECUTABLE} ${hlsl} -o ${out_dir}/${out_name} -I ${YAMIPAN_HLSL_DIR}
            DEPENDS ${hlsl} ${YAMIPAN_HLSL_INCLUDES}
            COMMENT "shadercross ${name} -> ${directory}/${out_name}"
            VERBATIM)
        # copy_directory only runs on a relink: a shader-only change must still reach the app.
        add_custom_command(
            OUTPUT ${run_dir}/${out_name}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${run_dir}
            COMMAND ${CMAKE_COMMAND} -E copy ${out_dir}/${out_name} ${run_dir}/${out_name}
            DEPENDS ${out_dir}/${out_name}
            VERBATIM)
        list(APPEND YAMIPAN_SHADER_OUTPUTS ${run_dir}/${out_name})
    endforeach()
endforeach()
add_custom_target(compile_shaders DEPENDS ${YAMIPAN_SHADER_OUTPUTS})
