#include <Mizu/Mizu.h>

class BaseShader : public Mizu::ShaderDeclaration<> {
  public:
    // clang-format off
    BEGIN_SHADER_PARAMETERS()
        SHADER_PARAMETER_RG_UNIFORM_BUFFER(uCameraInfo)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class PBRMaterialShader : public Mizu::MaterialShader<BaseShader> {
  public:
    IMPLEMENT_GRAPHICS_SHADER("/ExampleShadersPath/PBRShader.vert.spv",
                              "main",
                              "/ExampleShadersPath/PBRShader.frag.spv",
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
        const auto example_path = std::filesystem::path(MIZU_EXAMPLE_PATH);
        Mizu::ShaderManager::create_shader_mapping("/ExampleShadersPath", example_path / "shaders");

        Mizu::ImageDescription desc{};
        desc.usage = Mizu::ImageUsageBits::Sampled;

        PBRMaterialShader::MaterialParameters params{};
        params.albedo = Mizu::Texture2D::create(desc);
        params.metallic = Mizu::Texture2D::create(desc);

        Mizu::Material<PBRMaterialShader> mat;
        MIZU_ASSERT(mat.init(params), "Could not init material");
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
