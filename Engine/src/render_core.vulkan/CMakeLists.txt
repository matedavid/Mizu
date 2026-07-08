project(Engine.RenderCore.Vulkan LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED)

generate_export_header(${PROJECT_NAME} 
    EXPORT_FILE_NAME "mizu_render_core_vulkan_module.h"
    EXPORT_MACRO_NAME "MIZU_RENDER_CORE_VULKAN_API"
)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME} mizu_render_core_vulkan_module)

target_link_libraries(${PROJECT_NAME} PRIVATE Engine.Base)
target_link_libraries(${PROJECT_NAME} PRIVATE Engine.RenderCoreApi)

target_compile_definitions(${PROJECT_NAME} PRIVATE MIZU_VULKAN_VALIDATIONS_ENABLED=${MIZU_DEBUG})

#
# Dependencies
#

# Vulkan
find_package(Vulkan)

if (Vulkan_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC ${Vulkan_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
endif ()

