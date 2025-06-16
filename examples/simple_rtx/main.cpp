#include <Mizu/Mizu.h>

#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>
#include <Mizu/Extensions/ImGui.h>

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 1;

class RayTracingShader : public Mizu::RayTracingShaderDeclaration
{
  public:
    ShaderDescription get_shader_description() const override
    {
        Mizu::Shader::Description raygen_desc{};
        raygen_desc.path = "/ExampleShadersPath/Example.raygen.spv";
        raygen_desc.entry_point = "rtxRaygen";
        raygen_desc.type = Mizu::ShaderType::RtxRaygen;

        Mizu::Shader::Description miss_desc{};
        miss_desc.path = "/ExampleShadersPath/Example.miss.spv";
        miss_desc.entry_point = "rtxMiss";
        miss_desc.type = Mizu::ShaderType::RtxMiss;

        Mizu::Shader::Description closest_hit_desc{};
        closest_hit_desc.path = "/ExampleShadersPath/Example.closesthit.spv";
        closest_hit_desc.entry_point = "rtxClosestHit";
        closest_hit_desc.type = Mizu::ShaderType::RtxClosestHit;

        ShaderDescription desc{};
        desc.raygen = Mizu::ShaderManager::get_shader(raygen_desc);
        desc.miss = Mizu::ShaderManager::get_shader(miss_desc);
        desc.closest_hit = Mizu::ShaderManager::get_shader(closest_hit_desc);

        return desc;
    }

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_UNIFORM_BUFFER(cameraInfo)
        SHADER_PARAMETER_RG_STORAGE_IMAGE_VIEW(output)
        SHADER_PARAMETER_RG_TLAS(scene)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class ExampleLayer : public Mizu::Layer
{
  public:
    ExampleLayer()
    {
        const uint32_t width = Mizu::Application::instance()->get_window()->get_width();
        const uint32_t height = Mizu::Application::instance()->get_window()->get_height();

        const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
        m_camera_controller =
            std::make_unique<Mizu::FirstPersonCameraController>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera_controller->set_position({0.0f, 0.0f, 4.0f});
        m_camera_controller->set_config(Mizu::FirstPersonCameraController::Config{
            .lateral_movement_speed = 4.0f,
            .longitudinal_movement_speed = 4.0f,
            .lateral_rotation_sensitivity = 5.0f,
            .vertical_rotation_sensitivity = 5.0f,
            .rotate_modifier_key = Mizu::MouseButton::Right,
        });

        Mizu::ShaderManager::create_shader_mapping("/ExampleShadersPath", MIZU_EXAMPLE_SHADERS_PATH);

        m_imgui_presenter = std::make_unique<Mizu::ImGuiPresenter>(Mizu::Application::instance()->get_window());
        m_render_graph_allocator = Mizu::RenderGraphDeviceMemoryAllocator::create();

        m_command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_fences.resize(MAX_FRAMES_IN_FLIGHT);
        m_image_acquired_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_result_textures.resize(MAX_FRAMES_IN_FLIGHT);
        m_result_image_views.resize(MAX_FRAMES_IN_FLIGHT);
        m_imgui_textures.resize(MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_command_buffers[i] = Mizu::RenderCommandBuffer::create();

            m_fences[i] = Mizu::Fence::create();
            m_image_acquired_semaphores[i] = Mizu::Semaphore::create();
            m_render_finished_semaphores[i] = Mizu::Semaphore::create();

            Mizu::Texture2D::Description texture_desc{};
            texture_desc.dimensions = {width, height};
            texture_desc.format = Mizu::ImageFormat::RGBA8_UNORM;
            texture_desc.usage = Mizu::ImageUsageBits::Storage | Mizu::ImageUsageBits::Sampled;
            texture_desc.name = std::format("OutputTexture_{}", i);

            const auto result_texture = Mizu::Texture2D::create(texture_desc, Mizu::Renderer::get_allocator());
            const auto result_view = Mizu::ImageResourceView::create(result_texture->get_resource());

            m_result_textures[i] = result_texture;
            m_result_image_views[i] = result_view;

            const ImTextureID imgui_texture = m_imgui_presenter->add_texture(*result_view);
            m_imgui_textures[i] = imgui_texture;
        }

        create_scene();
    }

    ~ExampleLayer() override
    {
        Mizu::Renderer::wait_idle();

        for (ImTextureID texture_id : m_imgui_textures)
        {
            m_imgui_presenter->remove_texture(texture_id);
        }
    }

    void on_update(double ts) override
    {
        m_fences[m_current_frame]->wait_for();

        m_imgui_presenter->acquire_next_image(m_image_acquired_semaphores[m_current_frame], nullptr);
        const auto& image = m_result_textures[m_current_frame];

        Mizu::CommandBuffer& command = *m_command_buffers[m_current_frame];

        // Render
        m_camera_controller->update(ts);

        struct CameraInfoUBO
        {
            glm::mat4 viewProj;
            glm::mat4 viewInverse;
            glm::mat4 projInverse;
        };

        CameraInfoUBO camera_info{};
        camera_info.viewProj = m_camera_controller->projection_matrix() * m_camera_controller->view_matrix();
        camera_info.viewInverse = glm::inverse(m_camera_controller->view_matrix());
        camera_info.projInverse = glm::inverse(m_camera_controller->projection_matrix());

        Mizu::RenderGraphBuilder builder;

        const Mizu::RGUniformBufferRef camera_info_ref = builder.create_uniform_buffer(camera_info, "CameraInfo");
        const Mizu::RGTextureRef output_ref =
            builder.register_external_texture(*image,
                                              {.input_state = Mizu::ImageResourceState::Undefined,
                                               .output_state = Mizu::ImageResourceState::ShaderReadOnly});
        RayTracingShader shader;

        RayTracingShader::Parameters params{};
        params.cameraInfo = camera_info_ref;
        params.output = builder.create_image_view(output_ref);
        params.scene = builder.register_external_tlas(m_triangle_tlas);

        Mizu::add_rtx_pass(builder,
                           "TraceRays",
                           shader,
                           params,
                           [=](Mizu::CommandBuffer& command, [[maybe_unused]] const Mizu::RGPassResources& resources) {
                               command.trace_rays(
                                   {image->get_resource()->get_width(), image->get_resource()->get_height(), 1});
                           });

        const std::optional<Mizu::RenderGraph> graph = builder.compile(*m_render_graph_allocator);
        MIZU_ASSERT(graph.has_value(), "Could not compile RenderGraph");

        m_graph = *graph;

        Mizu::CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = m_fences[m_current_frame];
        submit_info.signal_semaphore = m_render_finished_semaphores[m_current_frame];
        submit_info.wait_semaphore = m_image_acquired_semaphores[m_current_frame];

        m_graph.execute(command, submit_info);

        // command.submit(submit_info);

        draw_imgui();
        m_imgui_presenter->set_background_texture(m_imgui_textures[m_current_frame]);

        m_imgui_presenter->render_imgui_and_present({m_render_finished_semaphores[m_current_frame]});

        m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

        // Fps calculation
        if (m_frame_num % 30 == 0)
        {
            constexpr float FPS_AVERAGE_ALPHA = 0.8f;
            m_fps = FPS_AVERAGE_ALPHA * (1.0f / ts) + (1.0f - FPS_AVERAGE_ALPHA) * m_fps;
        }

        m_frame_num += 1;
    }

    void draw_imgui()
    {
        ImGui::Begin("Config");
        {
            if (ImGui::CollapsingHeader("Stats", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("fps: %f", m_fps);
            }
        }
        ImGui::End();
    }

    void create_scene()
    {
        const std::vector<glm::vec3> cube_vertices = {
            // Front face (Z = 0.5)
            {-0.5f, -0.5f, 0.5f}, // 0
            {0.5f, -0.5f, 0.5f},  // 1
            {0.5f, 0.5f, 0.5f},   // 2
            {-0.5f, 0.5f, 0.5f},  // 3

            // Back face (Z = -0.5)
            {-0.5f, -0.5f, -0.5f}, // 4
            {0.5f, -0.5f, -0.5f},  // 5
            {0.5f, 0.5f, -0.5f},   // 6
            {-0.5f, 0.5f, -0.5f},  // 7
        };

        // clang-format off
        const std::vector<uint32_t> cube_indices = {
            // Front face
            0, 1, 2,  0, 2, 3,
            // Back face 
            4, 6, 5,  4, 7, 6,
            // Top face
            3, 2, 6,  3, 6, 7,
            // Bottom face
            4, 5, 1,  4, 1, 0,
            // Left face
            4, 0, 3,  4, 3, 7,
            // Right face
            1, 5, 6,  1, 6, 2,
        };
        // clang-format on

        const auto cube_vb = Mizu::VertexBuffer::create(cube_vertices, Mizu::Renderer::get_allocator());
        const auto cube_ib = Mizu::IndexBuffer::create(cube_indices, Mizu::Renderer::get_allocator());

        Mizu::BottomLevelAccelerationStructure::Description blas_desc{};
        blas_desc.vertex_buffer = cube_vb;
        blas_desc.index_buffer = cube_ib;
        blas_desc.name = "Cube BLAS";

        m_triangle_blas = Mizu::BottomLevelAccelerationStructure::create(blas_desc, Mizu::Renderer::get_allocator());

        Mizu::RenderCommandBuffer::submit_single_time(
            [this](Mizu::CommandBuffer& command) { command.build_blas(*m_triangle_blas); });

        Mizu::TopLevelAccelerationStructure::Description tlas_desc{};
        tlas_desc.instances = {
            {.blas = m_triangle_blas, .position = glm::vec3(0.0f)},
        };
        tlas_desc.name = "Cube TLAS";

        m_triangle_tlas = Mizu::TopLevelAccelerationStructure::create(tlas_desc, Mizu::Renderer::get_allocator());

        Mizu::RenderCommandBuffer::submit_single_time(
            [this](Mizu::CommandBuffer& command) { command.build_tlas(*m_triangle_tlas); });
    }

    void on_window_resized(Mizu::WindowResizedEvent& event) override
    {
        Mizu::Renderer::wait_idle();

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            Mizu::Texture2D::Description texture_desc{};
            texture_desc.dimensions = {event.get_width(), event.get_height()};
            texture_desc.format = Mizu::ImageFormat::RGBA8_UNORM;
            texture_desc.usage = Mizu::ImageUsageBits::Storage | Mizu::ImageUsageBits::Sampled;
            texture_desc.name = std::format("OutputTexture_{}", i);

            const auto result_texture = Mizu::Texture2D::create(texture_desc, Mizu::Renderer::get_allocator());
            const auto result_view = Mizu::ImageResourceView::create(result_texture->get_resource());

            m_result_textures[i] = result_texture;
            m_result_image_views[i] = result_view;

            m_imgui_presenter->remove_texture(m_imgui_textures[i]);

            const ImTextureID imgui_texture = m_imgui_presenter->add_texture(*result_view);
            m_imgui_textures[i] = imgui_texture;
        }

        m_camera_controller->set_aspect_ratio(static_cast<float>(event.get_width())
                                              / static_cast<float>(event.get_height()));
    }

  private:
    std::shared_ptr<Mizu::BottomLevelAccelerationStructure> m_triangle_blas;
    std::shared_ptr<Mizu::TopLevelAccelerationStructure> m_triangle_tlas;

    std::unique_ptr<Mizu::FirstPersonCameraController> m_camera_controller;

    std::vector<std::shared_ptr<Mizu::CommandBuffer>> m_command_buffers;
    std::shared_ptr<Mizu::RenderGraphDeviceMemoryAllocator> m_render_graph_allocator;
    std::vector<std::shared_ptr<Mizu::Fence>> m_fences;
    std::vector<std::shared_ptr<Mizu::Semaphore>> m_image_acquired_semaphores, m_render_finished_semaphores;

    uint32_t m_current_frame = 0;

    std::unique_ptr<Mizu::ImGuiPresenter> m_imgui_presenter;

    std::vector<std::shared_ptr<Mizu::Texture2D>> m_result_textures;
    std::vector<std::shared_ptr<Mizu::ImageResourceView>> m_result_image_views;
    std::vector<ImTextureID> m_imgui_textures;

    Mizu::RenderGraph m_graph;

    float m_fps = 1.0f;
    uint64_t m_frame_num = 0;
};

int main()
{
    Mizu::Application::Description desc{};
    desc.graphics_api = Mizu::GraphicsAPI::Vulkan;
    desc.name = "Simple Rtx Example";
    desc.width = 1920;
    desc.height = 1080;

    Mizu::Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
