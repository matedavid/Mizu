#include <Mizu/Mizu.h>

#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>

#include <format>
#include <glm/gtc/matrix_transform.hpp>

#include "simple_rtx_shaders.h"

using namespace Mizu;

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 1;

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

        ShaderManager::get().add_shader_mapping("/SimpleRtxShaders", MIZU_EXAMPLE_SHADERS_PATH);

        m_render_graph_transient_allocator = g_render_device->create_aliased_memory_allocator();
        m_render_graph_host_allocator = g_render_device->create_aliased_memory_allocator(true);

        SwapchainDescription swapchain_desc{};
        swapchain_desc.window = Application::instance()->get_window();
        swapchain_desc.format = ImageFormat::R8G8B8A8_UNORM;
        m_swapchain = g_render_device->create_swapchain(swapchain_desc);

        m_command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_fences.resize(MAX_FRAMES_IN_FLIGHT);
        m_image_acquired_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_command_buffers[i] = g_render_device->create_command_buffer(CommandBufferType::Graphics);

            m_fences[i] = g_render_device->create_fence();
            m_image_acquired_semaphores[i] = g_render_device->create_semaphore();
            m_render_finished_semaphores[i] = g_render_device->create_semaphore();
        }

        create_scene();
    }

    ~ExampleLayer() override { g_render_device->wait_idle(); }

    void on_update(double ts) override
    {
        m_fences[m_current_frame]->wait_for();

        m_swapchain->acquire_next_image(m_image_acquired_semaphores[m_current_frame], nullptr);
        const auto image = m_swapchain->get_image(m_swapchain->get_current_image_idx());

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

        CommandUtils::submit_single_time(CommandBufferType::Graphics, [=, this](CommandBuffer& command) {
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

        const RGBufferRef camera_info_ref = builder.create_constant_buffer(camera_info, "CameraInfo");
        const RGImageRef output_ref = builder.register_external_texture(
            image, {.input_state = ImageResourceState::Undefined, .output_state = ImageResourceState::ShaderReadOnly});

        const RGBufferRef vertices_ref =
            builder.register_external_structured_buffer(m_cube_vb, RGExternalBufferParams{});
        const RGBufferRef indices_ref =
            builder.register_external_structured_buffer(m_cube_ib, RGExternalBufferParams{});
        const RGBufferRef point_lights_ref =
            builder.create_structured_buffer<RtxPointLight>(point_lights, "PointLights");

        SimpleRtxParameters params{};
        params.cameraInfo = builder.create_buffer_cbv(camera_info_ref);
        params.output = builder.create_texture_uav(output_ref);
        params.scene = builder.register_external_acceleration_structure(m_cube_tlas);
        params.vertices = builder.create_buffer_srv(vertices_ref);
        params.indices = builder.create_buffer_srv(indices_ref);
        params.pointLights = builder.create_buffer_srv(point_lights_ref);

        RaygenShader raygen_shader{};
        MissShader miss_shader{};
        ShadowMissShader shadow_miss_shader{};
        ClosestHitShader closest_hit_shader{};

        RGResourceGroupLayout layout0{};
        layout0.add_resource(0, params.cameraInfo, ShaderType::RtxRaygen);
        layout0.add_resource(1, params.output, ShaderType::RtxRaygen);
        layout0.add_resource(2, params.scene, ShaderType::RtxRaygen | ShaderType::RtxClosestHit);

        RGResourceGroupLayout layout1{};
        layout1.add_resource(0, params.vertices, ShaderType::RtxClosestHit);
        layout1.add_resource(1, params.indices, ShaderType::RtxClosestHit);
        layout1.add_resource(2, params.pointLights, ShaderType::RtxClosestHit);

        const RGResourceGroupRef resource_group0_ref = builder.create_resource_group(layout0);
        const RGResourceGroupRef resource_group1_ref = builder.create_resource_group(layout1);

        builder.add_pass(
            "TraceRays", params, RGPassHint::RayTracing, [=](CommandBuffer& command, const RGPassResources& resources) {
                const auto pipeline = get_ray_tracing_pipeline(
                    raygen_shader.get_instance(),
                    {miss_shader.get_instance(), shadow_miss_shader.get_instance()},
                    {closest_hit_shader.get_instance()},
                    1);
                command.bind_pipeline(pipeline);

                bind_resource_group(command, resources, resource_group0_ref, 0);
                bind_resource_group(command, resources, resource_group1_ref, 1);

                command.trace_rays({image->get_width(), image->get_height(), 1});
            });

        const RenderGraphBuilderMemory builder_memory =
            RenderGraphBuilderMemory{*m_render_graph_transient_allocator, *m_render_graph_host_allocator};
        builder.compile(m_graph, builder_memory);

        CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = m_fences[m_current_frame];
        submit_info.signal_semaphores = {m_render_finished_semaphores[m_current_frame]};
        submit_info.wait_semaphores = {m_image_acquired_semaphores[m_current_frame]};

        m_graph.execute(command, submit_info);

        m_swapchain->present({m_render_finished_semaphores[m_current_frame]});

        m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

        // Fps calculation
        if (m_frame_num % 30 == 0)
        {
            constexpr double FPS_AVERAGE_ALPHA = 0.8;
            m_fps = FPS_AVERAGE_ALPHA * (1.0 / ts) + (1.f - FPS_AVERAGE_ALPHA) * m_fps;
        }

        m_frame_num += 1;
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
                            | BufferUsageBits::UnorderedAccess | BufferUsageBits::RtxAccelerationStructureInputReadOnly;
            vb_desc.name = "Cube VertexBuffer";

            BufferDescription ib_desc{};
            ib_desc.size = sizeof(uint32_t) * indices.size();
            ib_desc.usage = BufferUsageBits::IndexBuffer | BufferUsageBits::TransferDst
                            | BufferUsageBits::UnorderedAccess | BufferUsageBits::RtxAccelerationStructureInputReadOnly;
            ib_desc.name = "Cube IndexBuffer";

            m_cube_vb = BufferUtils::create_vertex_buffer(std::span<const RtxVertex>(vertices), "Cube VertexBuffer");
            m_cube_ib = BufferUtils::create_index_buffer(indices, "Cube IndexBuffer");
        }

        const auto triangles_geo = AccelerationStructureGeometry::triangles(
            m_cube_vb, ImageFormat::R32G32B32_SFLOAT, sizeof(RtxVertex), m_cube_ib);
        m_cube_blas = BottomLevelAccelerationStructure::create(triangles_geo, "Cube BLAS");

        const auto instances_geo = AccelerationStructureGeometry::instances(2, true);
        m_cube_tlas = TopLevelAccelerationStructure::create(instances_geo, "Cube TLAS");

        BufferDescription blas_scratch_buffer_desc{};
        blas_scratch_buffer_desc.size = glm::max(
            m_cube_blas->get_build_sizes().build_scratch_size, m_cube_tlas->get_build_sizes().build_scratch_size);
        blas_scratch_buffer_desc.usage =
            BufferUsageBits::RtxAccelerationStructureStorage | BufferUsageBits::UnorderedAccess;

        m_as_scratch_buffer = g_render_device->create_buffer(blas_scratch_buffer_desc);

        CommandUtils::submit_single_time(CommandBufferType::Graphics, [=, this](CommandBuffer& command) {
            command.build_blas(*m_cube_blas, *m_as_scratch_buffer);
        });

        CommandUtils::submit_single_time(CommandBufferType::Graphics, [=, this](CommandBuffer& command) {
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
        g_render_device->wait_idle();

        m_camera_controller->set_aspect_ratio(
            static_cast<float>(event.get_width()) / static_cast<float>(event.get_height()));
    }

  private:
    std::shared_ptr<BufferResource> m_cube_vb;
    std::shared_ptr<BufferResource> m_cube_ib;

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

    std::shared_ptr<Swapchain> m_swapchain;

    uint32_t m_current_frame = 0;
    RenderGraph m_graph;

    double m_fps = 1.0;
    uint64_t m_frame_num = 0;
};

int main()
{
    Application::Description desc{};
    desc.graphics_api = GraphicsApi::Vulkan;
    desc.name = "Simple Rtx Example";
    desc.width = 1920;
    desc.height = 1080;

    Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}
