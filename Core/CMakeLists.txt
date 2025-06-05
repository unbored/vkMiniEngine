set(MODULE_NAME Core)

file(GLOB_RECURSE MODULE_SOURCE
    "*.h"
    "*.c"
    "*.cpp"
)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(tinyutf8 CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)

set(MODULE_LIBRARIES
    glfw
    glm::glm
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
    ZLIB::ZLIB
    tinyutf8::tinyutf8
    KTX::ktx
)

# file(GLOB_RECURSE SHADER_SOURCE
#     "Shaders/*.vert"
#     "Shaders/*.frag"
#     "Shaders/*.tesc"
#     "Shaders/*.tese"
#     "Shaders/*.geom"
#     "Shaders/*.comp"
# )
# file (GLOB_RECURSE SHADER_GLSLS "Shaders/*.glsl")

# source_group(Shaders FILES ${SHADER_SOURCE} ${SHADER_GLSLS})

# set(SHADER_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/CompiledShaders)
# include_directories(
#     ${CMAKE_CURRENT_BINARY_DIR}
# )

message("Add Module ${MODULE_NAME}")

add_library(${MODULE_NAME} ${PROJECT_LINK_DYNAMIC_STATIC} ${MODULE_SOURCE} 
# ${SHADER_SOURCE} ${SHADER_GLSLS}
)
target_link_libraries(${MODULE_NAME} PRIVATE ${MODULE_LIBRARIES})

add_shaders(${MODULE_NAME})

# foreach(SHADER_FILE ${SHADER_SOURCE})
#     get_filename_component(FILE_NAME ${SHADER_FILE} NAME_WE)
#     set(OUTPUT_NAME ${SHADER_OUTPUT_PATH}/${FILE_NAME}.h)
#     add_custom_command(
#         OUTPUT ${OUTPUT_NAME}
#         COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}
#         ARGS -V --glsl-version 450 -l --vn g_${FILE_NAME} ${SHADER_FILE} -o ${OUTPUT_NAME}
#         DEPENDS ${SHADER_FILE} ${SHADER_GLSLS}
#         COMMENT "Compiling ${SHADER_FILE}"
#         VERBATIM
#     )
#     list(APPEND OUTPUT_NAMES ${OUTPUT_NAME})
# endforeach()
# add_custom_target(${MODULE_NAME}_Shader_Compile DEPENDS ${OUTPUT_NAMES})
# add_dependencies(${MODULE_NAME} ${MODULE_NAME}_Shader_Compile)
