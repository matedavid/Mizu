#include <Mizu/Mizu.h>

#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>

#include <glm/gtc/matrix_transform.hpp>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

// clang-format off
BEGIN_SHADER_PARAMETERS(BaseParameters)
    SHADER_PARAMETER_RG_UNIFORM_BUFFER(uCameraInfo)
END_SHADER_PARAMETERS()
// clang-format on

class TextureShader : public Mizu::GraphicsShaderDeclaration
{
  public:
    ShaderDescription get_shader_description() const override
    {
        Mizu::Shader::Description vs_desc{};
        vs_desc.path = "/ExampleShadersPath/TextureShader.vert.spv";
        vs_desc.entry_point = "vsMain";
        vs_desc.type = Mizu::ShaderType::Vertex;

        Mizu::Shader::Description fs_desc{};
        fs_desc.path = "/ExampleShadersPath/TextureShader.frag.spv";
        fs_desc.entry_point = "fsMain";
        fs_desc.type = Mizu::ShaderType::Fragment;

        ShaderDescription desc{};
        desc.vertex = Mizu::ShaderManager::get_shader2(vs_desc);
        desc.fragment = Mizu::ShaderManager::get_shader2(fs_desc);

        return desc;
    };

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseParameters)
        SHADER_PARAMETER_RG_IMAGE_VIEW(uTexture)
        SHADER_PARAMETER_SAMPLER_STATE(uTexture_Sampler)

        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class ComputeShader : public Mizu::ShaderDeclaration
{
  public:
    IMPLEMENT_SHADER("/ExampleShadersPath/PlasmaShader.comp.spv", "csMain", Mizu::ShaderType::Compute)

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_IMAGE_VIEW(uOutput)
    END_SHADER_PARAMETERS()
    // clang-format on
};

struct CameraUBO
{
    glm::mat4 view;
    glm::mat4 projection;
};

class ExampleLayer : public Mizu::Layer
{
  public:
    ExampleLayer()
    {
        const float aspect_ratio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        m_camera_controller =
            std::make_unique<Mizu::FirstPersonCameraController>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera_controller->set_position({0.0f, 0.0f, 4.0f});
        m_camera_controller->set_config(Mizu::FirstPersonCameraController::Config{
            .lateral_rotation_sensitivity = 5.0f,
            .vertical_rotation_sensitivity = 5.0f,
            .rotate_modifier_key = Mizu::MouseButton::Right,
        });

        m_scene = std::make_shared<Mizu::Scene>("Example Scene");

        const auto example_path = std::filesystem::path(MIZU_EXAMPLE_PATH);

        const auto mesh_path = example_path / "cube.fbx";

        auto loader = Mizu::AssimpLoader::load(mesh_path);
        MIZU_ASSERT(loader.has_value(), "Failed to load: {}", mesh_path.string());

        auto mesh_1 = m_scene->create_entity();
        mesh_1.add_component(Mizu::MeshRendererComponent{
            .mesh = loader->get_meshes()[0],
        });
        mesh_1.get_component<Mizu::TransformComponent>().rotation = glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f);

        Mizu::ShaderManager::create_shader_mapping("/ExampleShadersPath", MIZU_EXAMPLE_SHADERS_PATH);

        m_camera_ubo = Mizu::UniformBuffer::create<CameraUBO>(Mizu::Renderer::get_allocator(), "CameraInfo");

        m_fence = Mizu::Fence::create();
        m_image_acquired_semaphore = Mizu::Semaphore::create();
        m_render_finished_semaphore = Mizu::Semaphore::create();

        m_command_buffer = Mizu::RenderCommandBuffer::create();
        m_render_graph_allocator = Mizu::RenderGraphDeviceMemoryAllocator::create();

        m_swapchain = Mizu::Swapchain::create(Mizu::Application::instance()->get_window());
    }

    ~ExampleLayer() { Mizu::Renderer::wait_idle(); }

    void on_update(double ts) override
    {
        m_fence->wait_for();

        m_swapchain->acquire_next_image(m_image_acquired_semaphore, nullptr);
        const auto image = m_swapchain->get_image(m_swapchain->get_current_image_idx());

        m_time += static_cast<float>(ts);

        m_camera_controller->update(ts);
        m_camera_ubo->update(CameraUBO{
            .view = m_camera_controller->view_matrix(),
            .projection = m_camera_controller->projection_matrix(),
        });

        // Define RenderGraph

        const uint32_t width = image->get_resource()->get_width();
        const uint32_t height = image->get_resource()->get_height();

        Mizu::RenderGraphBuilder builder;

        const Mizu::RGTextureRef plasma_texture_ref =
            builder.create_texture<Mizu::Texture2D>({width, height}, Mizu::ImageFormat::RGBA8_UNORM, "PlasmaTexture");
        const Mizu::RGImageViewRef plasma_texture_view_ref = builder.create_image_view(plasma_texture_ref);

        ComputeShader::Parameters compute_params;
        compute_params.uOutput = plasma_texture_view_ref;

        const Mizu::RGResourceGroupRef compute_rg_ref = builder.create_resource_group(
            Mizu::RGResourceGroupLayout().add_resource(0, plasma_texture_view_ref, Mizu::ShaderType::Compute));

        ComputeShader compute_shader{};

        Mizu::ComputePipeline::Description desc{};
        desc.shader = ComputeShader::get_shader2();

        auto compute_pipeline = Mizu::ComputePipeline::create(desc);

        builder.add_pass("CreatePlasma",
                         compute_params,
                         Mizu::RGPassHint::Compute,
                         [=](Mizu::RenderCommandBuffer& command, const Mizu::RGPassResources resources) {
                             command.bind_pipeline(compute_pipeline);

                             const auto rg = resources.get_resource_group(compute_rg_ref);
                             command.bind_resource_group(rg, 0);

                             struct ComputeShaderConstant
                             {
                                 uint32_t width;
                                 uint32_t height;
                                 float time;
                             };

                             const ComputeShaderConstant constant_info{
                                 .width = width,
                                 .height = height,
                                 .time = m_time,
                             };

                             constexpr uint32_t LOCAL_SIZE = 16;
                             const auto group_count = glm::uvec3(
                                 (width + LOCAL_SIZE - 1) / LOCAL_SIZE, (height + LOCAL_SIZE - 1) / LOCAL_SIZE, 1);

                             command.push_constant("uPlasmaInfo", constant_info);
                             command.dispatch(group_count);
                         });

        const Mizu::RGTextureRef present_texture_ref =
            builder.register_external_texture(*image, {.output_state = Mizu::ImageResourceState::Present});
        const Mizu::RGImageViewRef present_texture_view_ref = builder.create_image_view(present_texture_ref);

        const Mizu::RGTextureRef depth_texture_ref =
            builder.create_texture<Mizu::Texture2D>({width, height}, Mizu::ImageFormat::D32_SFLOAT, "DepthTexture");
        const Mizu::RGImageViewRef depth_texture_view_ref = builder.create_image_view(depth_texture_ref);

        const Mizu::RGUniformBufferRef camera_ubo_ref = builder.register_external_buffer(*m_camera_ubo);

        TextureShader texture_shader;

        TextureShader::Parameters texture_pass_params;
        texture_pass_params.uCameraInfo = camera_ubo_ref;
        texture_pass_params.uTexture = plasma_texture_view_ref;
        texture_pass_params.uTexture_Sampler = Mizu::RHIHelpers::get_sampler_state(Mizu::SamplingOptions{});
        texture_pass_params.framebuffer.width = width;
        texture_pass_params.framebuffer.height = height;
        texture_pass_params.framebuffer.color_attachments = {present_texture_view_ref};
        texture_pass_params.framebuffer.depth_stencil_attachment = depth_texture_view_ref;

        Mizu::GraphicsPipeline::Description texture_pipeline_desc{};
        texture_pipeline_desc.depth_stencil.depth_test = true;
        texture_pipeline_desc.depth_stencil.depth_write = true;

        Mizu::add_graphics_pass(
            builder,
            "TexturePass",
            texture_shader,
            texture_pass_params,
            texture_pipeline_desc,
            [=](Mizu::RenderCommandBuffer& command, const Mizu::RGPassResources resources) {
                struct ModelInfoData
                {
                    glm::mat4 model;
                };

                for (const auto& entity : m_scene->view<Mizu::MeshRendererComponent>())
                {
                    const Mizu::MeshRendererComponent& mesh_renderer =
                        entity.get_component<Mizu::MeshRendererComponent>();
                    const Mizu::TransformComponent& transform = entity.get_component<Mizu::TransformComponent>();

                    glm::mat4 model(1.0f);
                    model = glm::translate(model, transform.position);
                    model = glm::rotate(model, transform.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
                    model = glm::rotate(model, transform.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
                    model = glm::rotate(model, transform.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
                    model = glm::scale(model, transform.scale);

                    ModelInfoData model_info{};
                    model_info.model = model;
                    command.push_constant("uModelInfo", model_info);

                    command.draw_indexed(*mesh_renderer.mesh->vertex_buffer(), *mesh_renderer.mesh->index_buffer());
                }
            });

        const std::optional<Mizu::RenderGraph> graph = builder.compile(*m_render_graph_allocator);
        MIZU_ASSERT(graph.has_value(), "Could not compile RenderGraph");

        m_graph = *graph;

        Mizu::CommandBufferSubmitInfo submit_info{};
        submit_info.wait_semaphore = m_image_acquired_semaphore;
        submit_info.signal_semaphore = m_render_finished_semaphore;
        submit_info.signal_fence = m_fence;
        m_graph.execute(*m_command_buffer, submit_info);

        m_swapchain->present({m_render_finished_semaphore});
    }

    void on_window_resized(Mizu::WindowResizedEvent& event) override
    {
        m_camera_controller->set_aspect_ratio(static_cast<float>(event.get_width())
                                              / static_cast<float>(event.get_height()));
    }

  private:
    std::shared_ptr<Mizu::Scene> m_scene;
    std::unique_ptr<Mizu::FirstPersonCameraController> m_camera_controller;
    std::shared_ptr<Mizu::Swapchain> m_swapchain;

    std::shared_ptr<Mizu::UniformBuffer> m_camera_ubo;

    std::shared_ptr<Mizu::Fence> m_fence;
    std::shared_ptr<Mizu::Semaphore> m_image_acquired_semaphore, m_render_finished_semaphore;

    std::shared_ptr<Mizu::RenderCommandBuffer> m_command_buffer;
    std::shared_ptr<Mizu::RenderGraphDeviceMemoryAllocator> m_render_graph_allocator;

    Mizu::RenderGraph m_graph;

    float m_time = 0.0f;
};

int main()
{
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Plasma";
    desc.width = WIDTH;
    desc.height = HEIGHT;

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
