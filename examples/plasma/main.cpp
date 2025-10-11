#include <Mizu/Mizu.h>

#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace Mizu;

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

class TextureShader : public GraphicsShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER_DECLARATION(
        "/ExampleShadersPath/TextureShader.vert.spv",
        "vsMain",
        "/ExampleShadersPath/TextureShader.frag.spv",
        "fsMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS_INHERIT(Parameters, BaseParameters)
        SHADER_PARAMETER_RG_SAMPLED_IMAGE_VIEW(uTexture)
        SHADER_PARAMETER_SAMPLER_STATE(uTexture_Sampler)

        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on
};

class ComputeShader : public ComputeShaderDeclaration
{
  public:
    IMPLEMENT_COMPUTE_SHADER_DECLARATION("/ExampleShadersPath/PlasmaShader.comp.spv", "csMain")

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_STORAGE_IMAGE_VIEW(uOutput)
    END_SHADER_PARAMETERS()
    // clang-format on
};

struct CameraUBO
{
    glm::mat4 view;
    glm::mat4 projection;
};

class ExampleLayer : public Layer
{
  public:
    void on_init() override
    {
        const float aspect_ratio = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);
        m_camera_controller =
            std::make_unique<FirstPersonCameraController>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera_controller->set_position({0.0f, 0.0f, 4.0f});
        m_camera_controller->set_config(FirstPersonCameraController::Config{
            .lateral_rotation_sensitivity = 5.0f,
            .vertical_rotation_sensitivity = 5.0f,
            .rotate_modifier_key = MouseButton::Right,
        });

        m_scene = std::make_shared<Scene>("Example Scene");

        const auto example_path = std::filesystem::path(MIZU_EXAMPLE_PATH);

        const auto mesh_path = example_path / "cube.fbx";

        auto loader = AssimpLoader::load(mesh_path);
        MIZU_ASSERT(loader.has_value(), "Failed to load: {}", mesh_path.string());

        auto mesh_1 = m_scene->create_entity();
        mesh_1.add_component(MeshRendererComponent{
            .mesh = loader->get_meshes()[0],
            .material = nullptr,
        });
        mesh_1.get_component<TransformComponent>().rotation = glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f);

        ShaderManager::create_shader_mapping("/ExampleShadersPath", MIZU_EXAMPLE_SHADERS_PATH);

        m_camera_ubo = UniformBuffer::create<CameraUBO>("CameraInfo");

        m_fence = Fence::create();
        m_image_acquired_semaphore = Semaphore::create();
        m_render_finished_semaphore = Semaphore::create();

        m_command_buffer = RenderCommandBuffer::create();
        m_render_graph_transient_allocator = AliasedDeviceMemoryAllocator::create();
        m_render_graph_host_allocator = AliasedDeviceMemoryAllocator::create(true);

        m_swapchain = Swapchain::create(Application::instance()->get_window());
    }

    ~ExampleLayer() { Renderer::wait_idle(); }

    void on_update(double ts) override
    {
        m_fence->wait_for();

        m_swapchain->acquire_next_image(m_image_acquired_semaphore, nullptr);
        const auto image = m_swapchain->get_image(m_swapchain->get_current_image_idx());

        m_time += static_cast<float>(ts);

        m_camera_controller->update(ts);
        m_camera_ubo->update(CameraUBO{
            .view = m_camera_controller->get_view_matrix(),
            .projection = m_camera_controller->get_projection_matrix(),
        });

        // Define RenderGraph

        const uint32_t width = image->get_resource()->get_width();
        const uint32_t height = image->get_resource()->get_height();

        RenderGraphBuilder builder;

        const RGImageRef plasma_texture_ref =
            builder.create_texture<Texture2D>({width, height}, ImageFormat::RGBA8_UNORM, "PlasmaTexture");
        const RGImageViewRef plasma_texture_view_ref = builder.create_image_view(plasma_texture_ref);

        ComputeShader::Parameters compute_params;
        compute_params.uOutput = plasma_texture_view_ref;

        const ComputePipeline::Description pipeline_desc =
            ComputeShader::get_pipeline_template(ComputeShader{}.get_shader_description());

        add_compute_pass(
            builder,
            "CreatePlasma",
            compute_params,
            pipeline_desc,
            [=, this](CommandBuffer& command, [[maybe_unused]] const RGPassResources resources) {
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
                const auto group_count =
                    glm::uvec3((width + LOCAL_SIZE - 1) / LOCAL_SIZE, (height + LOCAL_SIZE - 1) / LOCAL_SIZE, 1);

                command.push_constant("uPlasmaInfo", constant_info);
                command.dispatch(group_count);
            });

        const RGImageRef present_texture_ref =
            builder.register_external_texture(*image, {.output_state = ImageResourceState::Present});
        const RGImageViewRef present_texture_view_ref = builder.create_image_view(present_texture_ref);

        const RGImageRef depth_texture_ref =
            builder.create_texture<Texture2D>({width, height}, ImageFormat::D32_SFLOAT, "DepthTexture");
        const RGImageViewRef depth_texture_view_ref = builder.create_image_view(depth_texture_ref);

        const RGUniformBufferRef camera_ubo_ref = builder.register_external_buffer(*m_camera_ubo);

        TextureShader texture_shader;

        TextureShader::Parameters texture_pass_params;
        texture_pass_params.uCameraInfo = camera_ubo_ref;
        texture_pass_params.uTexture = plasma_texture_view_ref;
        texture_pass_params.uTexture_Sampler = RHIHelpers::get_sampler_state(SamplingOptions{});
        texture_pass_params.framebuffer.width = width;
        texture_pass_params.framebuffer.height = height;
        texture_pass_params.framebuffer.color_attachments = {present_texture_view_ref};
        texture_pass_params.framebuffer.depth_stencil_attachment = depth_texture_view_ref;

        GraphicsPipeline::Description texture_pipeline_desc =
            TextureShader::get_pipeline_template(TextureShader{}.get_shader_description());
        texture_pipeline_desc.depth_stencil.depth_test = true;
        texture_pipeline_desc.depth_stencil.depth_write = true;

        add_raster_pass(
            builder,
            "TexturePass",
            texture_pass_params,
            texture_pipeline_desc,
            [=, this](CommandBuffer& command, [[maybe_unused]] const RGPassResources resources) {
                struct ModelInfoData
                {
                    glm::mat4 model;
                };

                for (const auto& entity : m_scene->view<MeshRendererComponent>())
                {
                    const MeshRendererComponent& mesh_renderer = entity.get_component<MeshRendererComponent>();
                    const TransformComponent& transform = entity.get_component<TransformComponent>();

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

        const RenderGraphBuilderMemory builder_memory =
            RenderGraphBuilderMemory{*m_render_graph_transient_allocator, *m_render_graph_host_allocator};
        builder.compile(m_graph, builder_memory);

        CommandBufferSubmitInfo submit_info{};
        submit_info.wait_semaphores = {m_image_acquired_semaphore};
        submit_info.signal_semaphores = {m_render_finished_semaphore};
        submit_info.signal_fence = m_fence;
        m_graph.execute(*m_command_buffer, submit_info);

        m_swapchain->present({m_render_finished_semaphore});
    }

    void on_window_resized(WindowResizedEvent& event) override
    {
        m_camera_controller->set_aspect_ratio(
            static_cast<float>(event.get_width()) / static_cast<float>(event.get_height()));
    }

  private:
    std::shared_ptr<Scene> m_scene;
    std::unique_ptr<FirstPersonCameraController> m_camera_controller;
    std::shared_ptr<Swapchain> m_swapchain;

    std::shared_ptr<UniformBuffer> m_camera_ubo;

    std::shared_ptr<Fence> m_fence;
    std::shared_ptr<Semaphore> m_image_acquired_semaphore, m_render_finished_semaphore;

    std::shared_ptr<CommandBuffer> m_command_buffer;
    std::shared_ptr<AliasedDeviceMemoryAllocator> m_render_graph_transient_allocator;
    std::shared_ptr<AliasedDeviceMemoryAllocator> m_render_graph_host_allocator;

    RenderGraph m_graph;

    float m_time = 0.0f;
};

int main()
{
    Application::Description desc{};
    desc.graphics_api = GraphicsAPI::Vulkan;
    desc.name = "Plasma";
    desc.width = WIDTH;
    desc.height = HEIGHT;

    Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
