#include "shader/shader_registry.h"

#include "imgui.pipeline/imgui_shaders.h"

using namespace Mizu;

class ImGuiShaderProvider : public IShaderProvider
{
  public:
    void register_shaders(ShaderRegistry& registry) override
    {
        registry.add_shader_mapping("imgui", MIZU_ENGINE_EXTENSION_IMGUI_PATH);

        registry.register_shader<ImGuiShaderVS>();
        registry.register_shader<ImGuiShaderFS>();
    }
};

MIZU_REGISTER_SHADER_PROVIDER(ImGuiShaderProvider);
