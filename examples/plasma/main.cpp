#include <Mizu/Mizu.h>

#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace Mizu;

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

#include "plasma_shaders.h"

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

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

        ShaderManager::get().add_shader_mapping("/PlasmaExampleShaders", MIZU_EXAMPLE_SHADERS_PATH);

        m_camera_ubo = ConstantBuffer::create<CameraUBO>("CameraInfo");

        m_fence = Fence::create();
        m_image_acquired_semaphore = Semaphore::create();
        m_render_finished_semaphore = Semaphore::create();

        m_command_buffer = RenderCommandBuffer::create();
        m_render_graph_transient_allocator = AliasedDeviceMemoryAllocator::create();
        m_render_graph_host_allocator = AliasedDeviceMemoryAllocator::create(true);

        SwapchainDescription swapchain_desc{};
        swapchain_desc.window = Application::instance()->get_window();
        swapchain_desc.format = ImageFormat::R8G8B8A8_UNORM;

        m_swapchain = Swapchain::create(swapchain_desc);
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
            builder.create_texture<Texture2D>({width, height}, ImageFormat::R8G8B8A8_UNORM, "PlasmaTexture");

        ComputeShaderCS::Parameters compute_params{};
        compute_params.uOutput = builder.create_texture_uav(plasma_texture_ref);

        ComputeShaderCS compute_shader;

        ComputePipelineDescription compute_pipeline_desc{};
        compute_pipeline_desc.compute_shader = compute_shader.get_shader();

        RGResourceGroupLayout compute_layout{};
        compute_layout.add_resource(0, compute_params.uOutput, ShaderType::Compute);

        const RGResourceGroupRef compute_resource_group_ref = builder.create_resource_group(compute_layout);

        builder.add_pass(
            "CreatePlasma",
            compute_params,
            RGPassHint::Compute,
            [=, this](CommandBuffer& command, const RGPassResources resources) {
                const auto pipeline = get_compute_pipeline(compute_shader);
                command.bind_pipeline(pipeline);

                bind_resource_group(command, resources, compute_resource_group_ref, 0);

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

                command.push_constant(constant_info);
                command.dispatch(group_count);
            });

        const RGImageRef present_texture_ref = builder.register_external_texture(
            *image, {.input_state = ImageResourceState::Undefined, .output_state = ImageResourceState::Present});
        const RGTextureRtvRef present_texture_view_ref = builder.create_texture_rtv(present_texture_ref);

        const RGImageRef depth_texture_ref =
            builder.create_texture<Texture2D>({width, height}, ImageFormat::D32_SFLOAT, "DepthTexture");
        const RGTextureRtvRef depth_texture_view_ref = builder.create_texture_rtv(depth_texture_ref);

        const RGBufferRef camera_ubo_ref = builder.register_external_constant_buffer(
            *m_camera_ubo,
            {.input_state = BufferResourceState::ShaderReadOnly, .output_state = BufferResourceState::ShaderReadOnly});

        TextureShaderParameters texture_pass_params{};
        texture_pass_params.uCameraInfo = builder.create_buffer_cbv(camera_ubo_ref);
        texture_pass_params.uTexture = builder.create_texture_srv(plasma_texture_ref);
        texture_pass_params.uTexture_Sampler = get_sampler_state(SamplerStateDescription{});
        texture_pass_params.framebuffer = RGFramebufferAttachments{
            .width = width,
            .height = height,
            .color_attachments = {present_texture_view_ref},
            .depth_stencil_attachment = depth_texture_view_ref,
        };

        TextureShaderVS texture_vertex_shader;
        TextureShaderFS texture_fragment_shader;

        GraphicsPipelineDescription texture_pipeline_desc{};
        texture_pipeline_desc.vertex_shader = texture_vertex_shader.get_shader();
        texture_pipeline_desc.fragment_shader = texture_fragment_shader.get_shader();
        texture_pipeline_desc.depth_stencil.depth_test = true;
        texture_pipeline_desc.depth_stencil.depth_write = true;

        RGResourceGroupLayout texture_layout{};
        texture_layout.add_resource(0, texture_pass_params.uCameraInfo, ShaderType::Vertex);
        texture_layout.add_resource(1, texture_pass_params.uTexture, ShaderType::Fragment);
        texture_layout.add_resource(2, texture_pass_params.uTexture_Sampler, ShaderType::Fragment);

        const RGResourceGroupRef texture_resource_group_ref = builder.create_resource_group(texture_layout);

        builder.add_pass(
            "TexturePass",
            texture_pass_params,
            RGPassHint::Raster,
            [=, this](CommandBuffer& command, [[maybe_unused]] const RGPassResources resources) {
                const auto framebuffer = resources.get_framebuffer();
                command.begin_render_pass(framebuffer);
                {
                    const auto pipeline = get_graphics_pipeline(
                        texture_vertex_shader,
                        texture_fragment_shader,
                        texture_pipeline_desc.rasterization,
                        texture_pipeline_desc.depth_stencil,
                        texture_pipeline_desc.color_blend,
                        framebuffer);
                    command.bind_pipeline(pipeline);

                    bind_resource_group(command, resources, texture_resource_group_ref, 0);

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
                        command.push_constant(model_info);

                        command.draw_indexed(*mesh_renderer.mesh->vertex_buffer(), *mesh_renderer.mesh->index_buffer());
                    }
                }
                command.end_render_pass();
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

    std::shared_ptr<ConstantBuffer> m_camera_ubo;

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
    desc.graphics_api = GraphicsApi::Vulkan;
    desc.name = "Plasma";
    desc.width = WIDTH;
    desc.height = HEIGHT;

    Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
