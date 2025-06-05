set(MODULE_NAME ModelViewer)

file(GLOB_RECURSE MODULE_SOURCE
    "*.h"
    "*.c"
    "*.cpp"
)

include_directories(
    ${PROJECT_SOURCE_DIR}/Core
    ${PROJECT_SOURCE_DIR}/Model
)

set(MODULE_LIBRARIES
    Core
    Model
    glm::glm
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/Bin)

message("Add Module ${MODULE_NAME}")

add_executable(${MODULE_NAME} ${MODULE_SOURCE})
target_link_libraries(${MODULE_NAME} PRIVATE ${MODULE_LIBRARIES})

if(MSVC)
set_target_properties(${MODULE_NAME} PROPERTIES
                      VS_DEBUGGER_WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/Bin)
elseif(XCODE)
set_target_properties(${MODULE_NAME} PROPERTIES
                      XCODE_GENERATE_SCHEME TRUE
                      XCODE_SCHEME_WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/Bin)
endif()
