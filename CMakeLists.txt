cmake_minimum_required(VERSION 3.7)

project(MiniEngine)

set(CMAKE_CXX_STANDARD 17)

# configure utf8 for vs. and disable minmax
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX)
  add_compile_options("/utf-8")
  add_compile_options("/utf-8")
endif()

# set(CMAKE_BUILD_TYPE Debug)
option(PROJECT_LIBRARIES_STATIC "Compile static libraries" ON)
if(PROJECT_LIBRARIES_STATIC)
    set(PROJECT_LINK_DYNAMIC_STATIC "STATIC")
else()
    set(PROJECT_LINK_DYNAMIC_STATIC "SHARED")
endif()

if(WIN32)
    add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
elseif(APPLE)
    add_definitions(-DVK_USE_PLATFORM_METAL_EXT -DVK_ENABLE_BETA_EXTENSIONS)
endif()

add_definitions(-D_GAMING_DESKTOP)
add_definitions(-DGLM_FORCE_RADIANS -DGLM_FORCE_DEPTH_ZERO_TO_ONE)

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator-hpp CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# define a function for adding shader sources
function(add_shaders target)

file(GLOB_RECURSE SHADER_SOURCE
    "Shaders/*.vert"
    "Shaders/*.frag"
    "Shaders/*.tesc"
    "Shaders/*.tese"
    "Shaders/*.geom"
    "Shaders/*.comp"
)
file (GLOB_RECURSE SHADER_GLSLS "Shaders/*.glsl")

source_group(Shaders FILES ${SHADER_SOURCE} ${SHADER_GLSLS})

set(SHADER_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/CompiledShaders)
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(${target} PRIVATE ${SHADER_SOURCE} ${SHADER_GLSLS})

foreach(SHADER_FILE ${SHADER_SOURCE})
    get_filename_component(FILE_NAME ${SHADER_FILE} NAME_WE)
    set(OUTPUT_NAME ${SHADER_OUTPUT_PATH}/${FILE_NAME}.h)
    add_custom_command(
        OUTPUT ${OUTPUT_NAME}
        COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}
        ARGS -V -l --vn g_${FILE_NAME} ${SHADER_FILE} -o ${OUTPUT_NAME}
        DEPENDS ${SHADER_FILE} ${SHADER_GLSLS}
        COMMENT "Compiling ${SHADER_FILE}"
        VERBATIM
    )
    list(APPEND OUTPUT_NAMES ${OUTPUT_NAME})
endforeach()
add_custom_target(${target}_Shader_Compile DEPENDS ${OUTPUT_NAMES})
add_dependencies(${target} ${target}_Shader_Compile)

endfunction()


add_subdirectory(Core)
add_subdirectory(Model)
add_subdirectory(ModelViewer)
