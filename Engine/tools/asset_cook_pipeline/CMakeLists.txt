project(Tools.AssetCookPipeline LANGUAGES CXX)

add_library(${PROJECT_NAME} OBJECT)

mizu_configure_module(${PROJECT_NAME})
mizu_set_module_sources(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Engine.Base
    Engine.Asset
    Engine.RenderCore
    Engine.Core
    Engine.Shader
)

target_link_libraries(${PROJECT_NAME} PRIVATE assimp)

function(mizu_configure_asset_cook_pipeline target_name)
    add_executable(${target_name} $<TARGET_OBJECTS:Tools.AssetCookPipeline>)
    target_link_libraries(${target_name} PRIVATE
        Engine.Base
        Engine.Asset
        Engine.RenderCore
        Engine.Core
        Engine.Shader
    )

    target_link_libraries(${target_name} PRIVATE assimp)

    target_link_libraries(${target_name} PRIVATE MizuEngineDefines MizuPrivateOptions)
    set_target_properties(${target_name} PROPERTIES UNITY_BUILD ${MIZU_USE_UNITY_BUILD})
endfunction()

