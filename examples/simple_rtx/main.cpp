#include <Mizu/Mizu.h>

#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>
#include <Mizu/Extensions/ImGui.h>

#include <format>
#include <glm/gtc/matrix_transform.hpp>

using namespace Mizu;

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 1;

class RayTracingShader : public RayTracingShaderDeclaration
{
  public:
    ShaderDescription get_shader_description() const override
    {
        Shader::Description raygen_desc{};
        raygen_desc.path = "/ExampleShadersPath/Example.raygen.spv";
        raygen_desc.entry_point = "rtxRaygen";
        raygen_desc.type = ShaderType::RtxRaygen;

        Shader::Description miss_desc{};
        miss_desc.path = "/ExampleShadersPath/Example.miss.spv";
        miss_desc.entry_point = "rtxMiss";
        miss_desc.type = ShaderType::RtxMiss;

        Shader::Description shadow_miss_desc{};
        shadow_miss_desc.path = "/ExampleShadersPath/Example.shadow.miss.spv";
        shadow_miss_desc.entry_point = "rtxShadowMiss";
        shadow_miss_desc.type = ShaderType::RtxMiss;

        Shader::Description closest_hit_desc{};
        closest_hit_desc.path = "/ExampleShadersPath/Example.closesthit.spv";
        closest_hit_desc.entry_point = "rtxClosestHit";
        closest_hit_desc.type = ShaderType::RtxClosestHit;

        ShaderDescription desc{};
        desc.raygen = ShaderManager::get_shader(raygen_desc);
        desc.misses = {ShaderManager::get_shader(miss_desc), ShaderManager::get_shader(shadow_miss_desc)};
        desc.closest_hits = {ShaderManager::get_shader(closest_hit_desc)};

        return desc;
    }

    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_UNIFORM_BUFFER(cameraInfo)
        SHADER_PARAMETER_RG_STORAGE_IMAGE_VIEW(output)
        SHADER_PARAMETER_RG_ACCELERATION_STRUCTURE(scene)

        SHADER_PARAMETER_RG_STORAGE_BUFFER(vertices)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(indices)
        SHADER_PARAMETER_RG_STORAGE_BUFFER(pointLights)
    END_SHADER_PARAMETERS()
    // clang-format on
};

class ExampleLayer : public Layer
{
  public:
    void on_init() override
    {
        const uint32_t width = Application::instance()->get_window()->get_width();
        const uint32_t height = Application::instance()->get_window()->get_height();

        const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
        m_camera_controller =
            std::make_unique<FirstPersonCameraController>(glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f);
        m_camera_controller->set_position({0.0f, 0.0f, 4.0f});
        m_camera_controller->set_config(FirstPersonCameraController::Config{
            .lateral_movement_speed = 4.0f,
            .longitudinal_movement_speed = 4.0f,
            .lateral_rotation_sensitivity = 5.0f,
            .vertical_rotation_sensitivity = 5.0f,
            .rotate_modifier_key = MouseButton::Right,
        });

        ShaderManager::create_shader_mapping("/ExampleShadersPath", MIZU_EXAMPLE_SHADERS_PATH);

        m_imgui_presenter = std::make_unique<ImGuiPresenter>(Application::instance()->get_window());
        m_render_graph_transient_allocator = AliasedDeviceMemoryAllocator::create();
        m_render_graph_host_allocator = AliasedDeviceMemoryAllocator::create(true);

        m_command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_fences.resize(MAX_FRAMES_IN_FLIGHT);
        m_image_acquired_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_result_textures.resize(MAX_FRAMES_IN_FLIGHT);
        m_result_image_views.resize(MAX_FRAMES_IN_FLIGHT);
        m_imgui_textures.resize(MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_command_buffers[i] = RenderCommandBuffer::create();

            m_fences[i] = Fence::create();
            m_image_acquired_semaphores[i] = Semaphore::create();
            m_render_finished_semaphores[i] = Semaphore::create();

            Texture2D::Description texture_desc{};
            texture_desc.dimensions = {width, height};
            texture_desc.format = ImageFormat::RGBA8_UNORM;
            texture_desc.usage = ImageUsageBits::Storage | ImageUsageBits::Sampled;
            texture_desc.name = std::format("OutputTexture_{}", i);

            const auto result_texture = Texture2D::create(texture_desc);
            const auto result_view = ImageResourceView::create(result_texture->get_resource());

            m_result_textures[i] = result_texture;
            m_result_image_views[i] = result_view;

            const ImTextureID imgui_texture = m_imgui_presenter->add_texture(*result_view);
            m_imgui_textures[i] = imgui_texture;
        }

        create_scene();
    }

    ~ExampleLayer() override
    {
        Renderer::wait_idle();

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

        static float elapsed_time = 0;
        elapsed_time += (float)ts;

        std::vector<RtxPointLight> point_lights;
        for (const RtxPointLight& pl : m_point_lights)
        {
            RtxPointLight updated_point_light{};
            updated_point_light.position = glm::vec3(
                glm::rotate(glm::mat4(1.0f), glm::radians(elapsed_time * 10.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::vec4(pl.position, 1.0f));
            updated_point_light.radius = pl.radius;
            updated_point_light.color = pl.color;

            point_lights.push_back(updated_point_light);
        }

        RenderCommandBuffer::submit_single_time([=, this](CommandBuffer& command) {
            glm::mat4 cube_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f));
            cube_transform = glm::translate(cube_transform, glm::vec3(0.0f, glm::cos(elapsed_time) + 1.0f, 0.0f));
            cube_transform = glm::scale(cube_transform, glm::vec3(0.5f));

            glm::mat4 floor_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
            floor_transform = glm::scale(floor_transform, glm::vec3(4.0f * 0.5f, 0.15f * 0.5f, 4.0f * 0.5f));
            floor_transform =
                glm::rotate(floor_transform, glm::radians(elapsed_time * 3.0f), glm::vec3(0.0f, 1.0f, 0.0f));

            std::array<AccelerationStructureInstanceData, 2> instances = {
                AccelerationStructureInstanceData{
                    .blas = m_cube_blas,
                    .transform = cube_transform,
                },
                AccelerationStructureInstanceData{
                    .blas = m_cube_blas,
                    .transform = floor_transform,
                },
            };

            command.update_tlas(*m_cube_tlas, instances, *m_as_scratch_buffer);
        });

        CommandBuffer& command = *m_command_buffers[m_current_frame];

        // Render
        m_camera_controller->update(ts);

        struct CameraInfoUBO
        {
            glm::mat4 viewProj;
            glm::mat4 viewInverse;
            glm::mat4 projInverse;
        };

        CameraInfoUBO camera_info{};
        camera_info.viewProj = m_camera_controller->get_projection_matrix() * m_camera_controller->get_view_matrix();
        camera_info.viewInverse = glm::inverse(m_camera_controller->get_view_matrix());
        camera_info.projInverse = glm::inverse(m_camera_controller->get_projection_matrix());

        RenderGraphBuilder builder;

        const RGUniformBufferRef camera_info_ref = builder.create_uniform_buffer(camera_info, "CameraInfo");
        const RGImageRef output_ref = builder.register_external_texture(
            *image, {.input_state = ImageResourceState::Undefined, .output_state = ImageResourceState::ShaderReadOnly});
        RayTracingShader shader;

        RayTracingShader::Parameters params{};
        params.cameraInfo = camera_info_ref;
        params.output = builder.create_image_view(output_ref);
        params.scene = builder.register_external_acceleration_structure(m_cube_tlas);
        params.vertices = builder.register_external_buffer(StorageBuffer(m_cube_vb->get_resource()));
        params.indices = builder.register_external_buffer(StorageBuffer(m_cube_ib->get_resource()));
        params.pointLights = builder.create_storage_buffer(point_lights, "PointLights");

        add_rtx_pass(
            builder,
            "TraceRays",
            shader,
            params,
            [=](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
                command.trace_rays({image->get_resource()->get_width(), image->get_resource()->get_height(), 1});
            });

        const RenderGraphBuilderMemory builder_memory =
            RenderGraphBuilderMemory{*m_render_graph_transient_allocator, *m_render_graph_host_allocator};
        builder.compile(m_graph, builder_memory);

        CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = m_fences[m_current_frame];
        submit_info.signal_semaphores = {m_render_finished_semaphores[m_current_frame]};
        submit_info.wait_semaphores = {m_image_acquired_semaphores[m_current_frame]};

        m_graph.execute(command, submit_info);

        // command.submit(submit_info);

        draw_imgui();
        m_imgui_presenter->set_background_texture(m_imgui_textures[m_current_frame]);

        m_imgui_presenter->render_imgui_and_present({m_render_finished_semaphores[m_current_frame]});

        m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

        // Fps calculation
        if (m_frame_num % 30 == 0)
        {
            constexpr double FPS_AVERAGE_ALPHA = 0.8;
            m_fps = FPS_AVERAGE_ALPHA * (1.0 / ts) + (1.f - FPS_AVERAGE_ALPHA) * m_fps;
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
        struct RtxVertex
        {
            glm::vec3 position;
            float pad0;
            glm::vec3 normal;
            float pad1;
        };

        const std::vector<RtxVertex> vertices = {
            // Front face (normal: 0,0,-1)
            {{-1.0f, -1.0f, -1.0f}, 0, {0.0f, 0.0f, -1.0f}, 0},
            {{1.0f, -1.0f, -1.0f}, 0, {0.0f, 0.0f, -1.0f}, 0},
            {{1.0f, 1.0f, -1.0f}, 0, {0.0f, 0.0f, -1.0f}, 0},
            {{-1.0f, 1.0f, -1.0f}, 0, {0.0f, 0.0f, -1.0f}, 0},

            // Back face (normal: 0,0,1)
            {{1.0f, -1.0f, 1.0f}, 0, {0.0f, 0.0f, 1.0f}, 0},
            {{-1.0f, -1.0f, 1.0f}, 0, {0.0f, 0.0f, 1.0f}, 0},
            {{-1.0f, 1.0f, 1.0f}, 0, {0.0f, 0.0f, 1.0f}, 0},
            {{1.0f, 1.0f, 1.0f}, 0, {0.0f, 0.0f, 1.0f}, 0},

            // Left face (normal: -1,0,0)
            {{-1.0f, -1.0f, 1.0f}, 0, {-1.0f, 0.0f, 0.0f}, 0},
            {{-1.0f, -1.0f, -1.0f}, 0, {-1.0f, 0.0f, 0.0f}, 0},
            {{-1.0f, 1.0f, -1.0f}, 0, {-1.0f, 0.0f, 0.0f}, 0},
            {{-1.0f, 1.0f, 1.0f}, 0, {-1.0f, 0.0f, 0.0f}, 0},

            // Right face (normal: 1,0,0)
            {{1.0f, -1.0f, -1.0f}, 0, {1.0f, 0.0f, 0.0f}, 0},
            {{1.0f, -1.0f, 1.0f}, 0, {1.0f, 0.0f, 0.0f}, 0},
            {{1.0f, 1.0f, 1.0f}, 0, {1.0f, 0.0f, 0.0f}, 0},
            {{1.0f, 1.0f, -1.0f}, 0, {1.0f, 0.0f, 0.0f}, 0},

            // Top face (normal: 0,1,0)
            {{-1.0f, 1.0f, -1.0f}, 0, {0.0f, 1.0f, 0.0f}, 0},
            {{1.0f, 1.0f, -1.0f}, 0, {0.0f, 1.0f, 0.0f}, 0},
            {{1.0f, 1.0f, 1.0f}, 0, {0.0f, 1.0f, 0.0f}, 0},
            {{-1.0f, 1.0f, 1.0f}, 0, {0.0f, 1.0f, 0.0f}, 0},

            // Bottom face (normal: 0,-1,0)
            {{-1.0f, -1.0f, 1.0f}, 0, {0.0f, -1.0f, 0.0f}, 0},
            {{1.0f, -1.0f, 1.0f}, 0, {0.0f, -1.0f, 0.0f}, 0},
            {{1.0f, -1.0f, -1.0f}, 0, {0.0f, -1.0f, 0.0f}, 0},
            {{-1.0f, -1.0f, -1.0f}, 0, {0.0f, -1.0f, 0.0f}, 0},
        };

        // clang-format off
        const std::vector<uint32_t> indices = {
            // Front (0-3)
            0, 1, 2,  0, 2, 3,
            // Back (4-7)
            4, 5, 6,  4, 6, 7,
            // Left (8-11)
            8, 9, 10, 8, 10, 11,
            // Right (12-15)
            12, 13, 14, 12, 14, 15,
            // Top (16-19)
            16, 17, 18, 16, 18, 19,
            // Bottom (20-23)
            20, 21, 22, 20, 22, 23
        };
        // clang-format on

        {
            BufferDescription vb_desc{};
            vb_desc.size = sizeof(RtxVertex) * vertices.size();
            vb_desc.usage = BufferUsageBits::VertexBuffer | BufferUsageBits::TransferDst
                            | BufferUsageBits::StorageBuffer | BufferUsageBits::RtxAccelerationStructureInputReadOnly;
            vb_desc.name = "Cube VertexBuffer";

            BufferDescription ib_desc{};
            ib_desc.size = sizeof(uint32_t) * indices.size();
            ib_desc.usage = BufferUsageBits::IndexBuffer | BufferUsageBits::TransferDst | BufferUsageBits::StorageBuffer
                            | BufferUsageBits::RtxAccelerationStructureInputReadOnly;
            ib_desc.name = "Cube IndexBuffer";

            m_cube_vb = std::make_shared<VertexBuffer>(BufferResource::create(vb_desc), (uint32_t)sizeof(RtxVertex));
            m_cube_ib = std::make_shared<IndexBuffer>(BufferResource::create(ib_desc), (uint32_t)indices.size());

            const uint8_t* vb_data = reinterpret_cast<const uint8_t*>(vertices.data());
            const uint8_t* ib_data = reinterpret_cast<const uint8_t*>(indices.data());

            auto vb_staging = StagingBuffer::create(vb_desc.size, vb_data);
            auto ib_staging = StagingBuffer::create(ib_desc.size, ib_data);

            TransferCommandBuffer::submit_single_time([=, this](CommandBuffer& command) {
                command.copy_buffer_to_buffer(*vb_staging->get_resource(), *m_cube_vb->get_resource());
                command.copy_buffer_to_buffer(*ib_staging->get_resource(), *m_cube_ib->get_resource());
            });
        }

        const auto triangles_geo = AccelerationStructureGeometry::triangles(
            m_cube_vb, ImageFormat::RGB32_SFLOAT, sizeof(RtxVertex), m_cube_ib);
        m_cube_blas = BottomLevelAccelerationStructure::create(triangles_geo, "Cube BLAS");

        const auto instances_geo = AccelerationStructureGeometry::instances(2, true);
        m_cube_tlas = TopLevelAccelerationStructure::create(instances_geo, "Cube TLAS");

        BufferDescription blas_scratch_buffer_desc{};
        blas_scratch_buffer_desc.size = glm::max(
            m_cube_blas->get_build_sizes().build_scratch_size, m_cube_tlas->get_build_sizes().build_scratch_size);
        blas_scratch_buffer_desc.usage =
            BufferUsageBits::RtxAccelerationStructureStorage | BufferUsageBits::StorageBuffer;

        m_as_scratch_buffer = BufferResource::create(blas_scratch_buffer_desc);

        RenderCommandBuffer::submit_single_time(
            [=, this](CommandBuffer& command) { command.build_blas(*m_cube_blas, *m_as_scratch_buffer); });

        RenderCommandBuffer::submit_single_time([=, this](CommandBuffer& command) {
            glm::mat4 cube_transform = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));

            glm::mat4 floor_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
            floor_transform = glm::scale(floor_transform, glm::vec3(4.0f * 0.5f, 0.15f * 0.5f, 4.0f * 0.5f));

            std::array<AccelerationStructureInstanceData, 2> instances = {
                AccelerationStructureInstanceData{
                    .blas = m_cube_blas,
                    .transform = cube_transform,
                },
                AccelerationStructureInstanceData{
                    .blas = m_cube_blas,
                    .transform = floor_transform,
                },
            };

            command.build_tlas(*m_cube_tlas, instances, *m_as_scratch_buffer);
        });

        m_point_lights.push_back(RtxPointLight{
            .position = glm::vec3(2.0f, 3.0f, 0.0f),
            .radius = 1.0f,
            .color = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f),
        });

        m_point_lights.push_back(RtxPointLight{
            .position = glm::vec3(-2.0f, 3.0f, 0.0f),
            .radius = 1.0f,
            .color = glm::vec4(0.1f, 0.3f, 0.8f, 1.0f),
        });
    }

    void on_window_resized(WindowResizedEvent& event) override
    {
        Renderer::wait_idle();

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            Texture2D::Description texture_desc{};
            texture_desc.dimensions = {event.get_width(), event.get_height()};
            texture_desc.format = ImageFormat::RGBA8_UNORM;
            texture_desc.usage = ImageUsageBits::Storage | ImageUsageBits::Sampled;
            texture_desc.name = std::format("OutputTexture_{}", i);

            const auto result_texture = Texture2D::create(texture_desc);
            const auto result_view = ImageResourceView::create(result_texture->get_resource());

            m_result_textures[i] = result_texture;
            m_result_image_views[i] = result_view;

            m_imgui_presenter->remove_texture(m_imgui_textures[i]);

            const ImTextureID imgui_texture = m_imgui_presenter->add_texture(*result_view);
            m_imgui_textures[i] = imgui_texture;
        }

        m_camera_controller->set_aspect_ratio(
            static_cast<float>(event.get_width()) / static_cast<float>(event.get_height()));
    }

  private:
    std::shared_ptr<VertexBuffer> m_cube_vb;
    std::shared_ptr<IndexBuffer> m_cube_ib;

    std::shared_ptr<AccelerationStructure> m_cube_blas;
    std::shared_ptr<AccelerationStructure> m_cube_tlas;
    std::shared_ptr<BufferResource> m_as_scratch_buffer;

    struct RtxPointLight
    {
        glm::vec3 position;
        float radius;
        glm::vec4 color;
    };
    std::vector<RtxPointLight> m_point_lights;

    std::unique_ptr<FirstPersonCameraController> m_camera_controller;

    std::vector<std::shared_ptr<CommandBuffer>> m_command_buffers;
    std::shared_ptr<AliasedDeviceMemoryAllocator> m_render_graph_transient_allocator;
    std::shared_ptr<AliasedDeviceMemoryAllocator> m_render_graph_host_allocator;

    std::vector<std::shared_ptr<Fence>> m_fences;
    std::vector<std::shared_ptr<Semaphore>> m_image_acquired_semaphores, m_render_finished_semaphores;

    uint32_t m_current_frame = 0;

    std::unique_ptr<ImGuiPresenter> m_imgui_presenter;

    std::vector<std::shared_ptr<Texture2D>> m_result_textures;
    std::vector<std::shared_ptr<ImageResourceView>> m_result_image_views;
    std::vector<ImTextureID> m_imgui_textures;

    RenderGraph m_graph;

    double m_fps = 1.0;
    uint64_t m_frame_num = 0;
};

int main()
{
    Application::Description desc{};
    desc.graphics_api = GraphicsAPI::Vulkan;
    desc.name = "Simple Rtx Example";
    desc.width = 1920;
    desc.height = 1080;

    Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
