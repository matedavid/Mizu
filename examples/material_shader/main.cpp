#include <Mizu/Mizu.h>

class PBRMaterialShader : public Mizu::MaterialShader<> {
  public:
    IMPLEMENT_GRAPHICS_SHADER("/ExampleShaders/PBRShader.vert.spv",
                              "main",
                              "/ExampleShaders/PBRShader.frag.spv",
                              "main")

    // clang-format off
    BEGIN_MATERIAL_PARAMETERS()
        MATERIAL_PARAMETER_TEXTURE2D(albedo)
        MATERIAL_PARAMETER_TEXTURE2D(metallic)
    END_MATERIAL_PARAMETERS()
    // clang-format on
};

class ExampleLayer : public Mizu::Layer {
  public:
    ExampleLayer() {
        PBRMaterialShader::MaterialParameters params{};
        params.albedo = nullptr;
        params.metallic = nullptr;

        auto members = PBRMaterialShader::MaterialParameters::get_members(params);
        for (auto& [name, value] : members) {
            MIZU_LOG_INFO("{} = {}", name, (void*)std::get<std::shared_ptr<Mizu::Texture2D>>(value).get());
        }
    }

    void on_update(double ts) override {}
};

int main() {
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Material Shaders";
    desc.width = 1920;
    desc.height = 1080;

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
