set(MODULE_NAME Model)

file(GLOB_RECURSE MODULE_SOURCE
    "*.h"
    "*.c"
    "*.cpp"
)

include_directories(
    ${PROJECT_SOURCE_DIR}/Core
)

find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")

set(MODULE_LIBRARIES
    Core
    glm::glm
    Vulkan::Vulkan
    GPUOpen::VulkanMemoryAllocator
    unofficial::VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp
)

message("Add Module ${MODULE_NAME}")

add_library(${MODULE_NAME} ${PROJECT_LINK_DYNAMIC_STATIC} ${MODULE_SOURCE})
target_link_libraries(${MODULE_NAME} PRIVATE ${MODULE_LIBRARIES})
target_include_directories(${MODULE_NAME} PRIVATE ${STB_INCLUDE_DIRS})

add_shaders(${MODULE_NAME})