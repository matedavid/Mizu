#include <Mizu/Mizu.h>

#include <Mizu/Extensions/CameraControllers.h>

#include <glm/gtc/matrix_transform.hpp>

#include "copy_shaders.h"
#include "simple_rtx_shaders.h"

using namespace Mizu;

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

class SimpleRtxSimulation : public GameSimulation
{
  public:
    void init() override
    {
        const uint32_t width = g_game_context->get_window().get_width();
        const uint32_t height = g_game_context->get_window().get_height();

        const float aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
        m_camera_controller = FirstPersonCameraController{glm::radians(60.0f), aspect_ratio, 0.001f, 100.0f};
        m_camera_controller.set_position({0.0f, 0.0f, 4.0f});
        m_camera_controller.set_config(
            FirstPersonCameraController::Config{
                .lateral_movement_speed = 4.0f,
                .longitudinal_movement_speed = 4.0f,
                .lateral_rotation_sensitivity = 5.0f,
                .vertical_rotation_sensitivity = 5.0f,
                .rotate_modifier_key = MouseButton::Right,
            });
    }

    void update(double dt) override
    {
        m_camera_controller.update(dt);
        sim_set_camera_state(m_camera_controller);
    }

  private:
    FirstPersonCameraController m_camera_controller{};
};

class SimpleRtxRenderModule : public IRenderModule
{
  public:
    SimpleRtxRenderModule()
    {
        create_scene();

        struct FullscreenTriangleVertex
        {
            glm::vec3 position;
            glm::vec2 texCoord;
        };

        const std::vector<FullscreenTriangleVertex> vertices = {
            {{3.0f, -1.0f, 0.0f}, {2.0f, 0.0f}},
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
            {{-1.0f, 3.0f, 0.0f}, {0.0f, 2.0f}},
        };

        BufferDescription fullscreen_triangle_desc{};
        fullscreen_triangle_desc.size = sizeof(FullscreenTriangleVertex) * vertices.size();
        fullscreen_triangle_desc.stride = sizeof(FullscreenTriangleVertex);
        fullscreen_triangle_desc.usage = BufferUsageBits::VertexBuffer | BufferUsageBits::TransferDst;
        fullscreen_triangle_desc.name = "Fullscreen Triangle";

        m_fullscreen_triangle = g_render_device->create_buffer(fullscreen_triangle_desc);
        BufferUtils::initialize_buffer(
            *m_fullscreen_triangle, reinterpret_cast<const uint8_t*>(vertices.data()), fullscreen_triangle_desc.size);

        ShaderManager::get().add_shader_mapping("/SimpleRtxShaders", MIZU_ENGINE_SHADERS_PATH);
    }

    void build_render_graph(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
    {
        const FrameInfo& frame_info = blackboard.get<FrameInfo>();
        m_elapsed_time += static_cast<float>(frame_info.last_frame_time);

        const Camera& camera = rend_get_camera_state();

        std::vector<RtxPointLight> point_lights;
        for (const RtxPointLight& pl : m_point_lights)
        {
            RtxPointLight updated_point_light{};
            updated_point_light.position = glm::vec3(
                glm::rotate(glm::mat4(1.0f), glm::radians(m_elapsed_time * 10.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                * glm::vec4(pl.position, 1.0f));
            updated_point_light.radius = pl.radius;
            updated_point_light.color = pl.color;

            point_lights.push_back(updated_point_light);
        }

        struct CameraInfoUBO
        {
            glm::mat4 viewProj;
            glm::mat4 viewInverse;
            glm::mat4 projInverse;
        };

        CameraInfoUBO camera_info{};
        camera_info.viewProj = camera.get_projection_matrix() * camera.get_view_matrix();
        camera_info.viewInverse = glm::inverse(camera.get_view_matrix());
        camera_info.projInverse = glm::inverse(camera.get_projection_matrix());
        const RGBufferRef camera_info_ref = builder.create_constant_buffer(camera_info, "CameraInfo");

        const RGImageRef output_texture_ref =
            builder.create_texture({frame_info.width, frame_info.height}, ImageFormat::R8G8B8A8_UNORM, "Output");

        const RGBufferRef vertices_ref =
            builder.register_external_structured_buffer(m_cube_vb, RGExternalBufferParams{});
        const RGBufferRef indices_ref =
            builder.register_external_structured_buffer(m_cube_ib, RGExternalBufferParams{});
        const RGBufferRef point_lights_ref =
            builder.create_structured_buffer<RtxPointLight>(point_lights, "PointLights");

        // clang-format off
        BEGIN_SHADER_PARAMETERS(BuildAccelerationStructureParams)
        END_SHADER_PARAMETERS()
        // clang-format on

        BuildAccelerationStructureParams build_as_params{};

        builder.add_pass(
            "BuildAccelerationStructure",
            build_as_params,
            RGPassHint::Immediate,
            [=](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
                glm::mat4 cube_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f));
                cube_transform = glm::translate(cube_transform, glm::vec3(0.0f, glm::cos(m_elapsed_time) + 1.0f, 0.0f));
                cube_transform = glm::scale(cube_transform, glm::vec3(0.5f));

                glm::mat4 floor_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
                floor_transform = glm::scale(floor_transform, glm::vec3(4.0f * 0.5f, 0.15f * 0.5f, 4.0f * 0.5f));
                floor_transform =
                    glm::rotate(floor_transform, glm::radians(m_elapsed_time * 3.0f), glm::vec3(0.0f, 1.0f, 0.0f));

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

        // clang-format off
        BEGIN_SHADER_PARAMETERS(TraceRaysParameters)
            SHADER_PARAMETER_RG_BUFFER_CBV(cameraInfo)
            SHADER_PARAMETER_RG_TEXTURE_UAV(output)
            SHADER_PARAMETER_RG_ACCELERATION_STRUCTURE(scene)

            SHADER_PARAMETER_RG_BUFFER_SRV(vertices)
            SHADER_PARAMETER_RG_BUFFER_SRV(indices)
            SHADER_PARAMETER_RG_BUFFER_SRV(pointLights)
        END_SHADER_PARAMETERS()
        // clang-format on

        TraceRaysParameters trace_rays_params{};
        trace_rays_params.cameraInfo = builder.create_buffer_cbv(camera_info_ref);
        trace_rays_params.output = builder.create_texture_uav(output_texture_ref);
        trace_rays_params.scene = builder.register_external_acceleration_structure(m_cube_tlas);
        trace_rays_params.vertices = builder.create_buffer_srv(vertices_ref);
        trace_rays_params.indices = builder.create_buffer_srv(indices_ref);
        trace_rays_params.pointLights = builder.create_buffer_srv(point_lights_ref);

        RaygenShader raygen_shader{};
        MissShader miss_shader{};
        ShadowMissShader shadow_miss_shader{};
        ClosestHitShader closest_hit_shader{};

        builder.add_pass(
            "TraceRays",
            trace_rays_params,
            RGPassHint::RayTracing,
            [=](CommandBuffer& command, const RGPassResources& resources) {
                const auto pipeline = get_ray_tracing_pipeline(
                    raygen_shader.get_instance(),
                    {miss_shader.get_instance(), shadow_miss_shader.get_instance()},
                    {closest_hit_shader.get_instance()},
                    1);
                command.bind_pipeline(pipeline);

                // clang-format off
                MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(TraceRaysLayout0)
                    MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::RtxRaygen)
                    MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_UAV(0, 1, ShaderType::RtxRaygen)
                    MIZU_DESCRIPTOR_SET_LAYOUT_ACCELERATION_STRUCTURE(0, 1, ShaderType::RtxRaygen | ShaderType::RtxClosestHit)
                MIZU_END_DESCRIPTOR_SET_LAYOUT()
                // clang-format on

                std::array descriptor_set_writes_0 = {
                    WriteDescriptor::ConstantBuffer(0, resources.get_buffer_cbv(trace_rays_params.cameraInfo)),
                    WriteDescriptor::TextureUav(0, resources.get_texture_uav(trace_rays_params.output)),
                    WriteDescriptor::AccelerationStructure(
                        0, resources.get_acceleration_structure(trace_rays_params.scene)),
                };

                const auto transient_descriptor_set_0 = g_render_device->allocate_descriptor_set(
                    TraceRaysLayout0::get_layout(), DescriptorSetAllocationType::Transient);
                transient_descriptor_set_0->update(descriptor_set_writes_0);

                // clang-format off
                MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(TraceRaysLayout1)
                    MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::RtxClosestHit)
                    MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::RtxClosestHit)
                    MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(2, 1, ShaderType::RtxClosestHit)
                MIZU_END_DESCRIPTOR_SET_LAYOUT()
                // clang-format on

                std::array descriptor_set_writes_1 = {
                    WriteDescriptor::StructuredBufferSrv(0, resources.get_buffer_srv(trace_rays_params.vertices)),
                    WriteDescriptor::StructuredBufferSrv(1, resources.get_buffer_srv(trace_rays_params.indices)),
                    WriteDescriptor::StructuredBufferSrv(2, resources.get_buffer_srv(trace_rays_params.pointLights)),
                };

                const auto transient_descriptor_set_1 = g_render_device->allocate_descriptor_set(
                    TraceRaysLayout1::get_layout(), DescriptorSetAllocationType::Transient);
                transient_descriptor_set_1->update(descriptor_set_writes_1);

                command.bind_descriptor_set(transient_descriptor_set_0, 0);
                command.bind_descriptor_set(transient_descriptor_set_1, 1);

                command.trace_rays({frame_info.width, frame_info.height, 1});
            });

        // clang-format off
        BEGIN_SHADER_PARAMETERS(CopyTextureParameters)
            SHADER_PARAMETER_RG_TEXTURE_SRV(input)
            SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
        END_SHADER_PARAMETERS()
        // clang-format on

        CopyTextureParameters copy_texture_params{};
        copy_texture_params.input = builder.create_texture_srv(output_texture_ref);
        copy_texture_params.framebuffer = RGFramebufferAttachments{
            .width = frame_info.width,
            .height = frame_info.height,
            .color_attachments = {builder.create_texture_rtv(
                frame_info.output_texture_ref,
                ImageResourceViewDescription{.override_format = ImageFormat::R8G8B8A8_SRGB})},
        };

        CopyTextureVS copy_texture_vertex_shader{};
        CopyTextureFS copy_texture_fragment_shader{};

        RGResourceGroupLayout copy_texture_layout0{};
        copy_texture_layout0.add_resource(0, copy_texture_params.input, ShaderType::Fragment);
        copy_texture_layout0.add_resource(1, get_sampler_state(SamplerStateDescription{}), ShaderType::Fragment);
        const RGResourceGroupRef copy_resource_group0_ref = builder.create_resource_group(copy_texture_layout0);

        builder.add_pass(
            "CopyTexture",
            copy_texture_params,
            RGPassHint::Raster,
            [=, this](CommandBuffer& command, const RGPassResources& resources) {
                const auto framebuffer = resources.get_framebuffer();
                command.begin_render_pass(framebuffer);
                {
                    const auto pipeline = get_graphics_pipeline(
                        copy_texture_vertex_shader,
                        copy_texture_fragment_shader,
                        RasterizationState{},
                        DepthStencilState{},
                        ColorBlendState{},
                        framebuffer);
                    command.bind_pipeline(pipeline);

                    std::array layout = {
                        DescriptorItem::TextureSrv(0, 1, ShaderType::Fragment),
                        DescriptorItem::SamplerState(0, 1, ShaderType::Fragment),
                    };

                    std::array writes = {
                        WriteDescriptor::TextureSrv(0, resources.get_texture_srv(copy_texture_params.input)),
                        WriteDescriptor::SamplerState(0, get_sampler_state({})),
                    };

                    const auto descriptor_set =
                        g_render_device->allocate_descriptor_set(layout, DescriptorSetAllocationType::Transient);
                    descriptor_set->update(writes);

                    command.bind_descriptor_set(descriptor_set, 0);

                    command.draw(*m_fullscreen_triangle);
                }
                command.end_render_pass();
            });
    }

  private:
    float m_elapsed_time = 0.0;

    std::shared_ptr<BufferResource> m_fullscreen_triangle;

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

        AccelerationStructureDescription cube_blas_desc{};
        cube_blas_desc.type = AccelerationStructureType::BottomLevel;
        cube_blas_desc.geometry = {triangles_geo};
        cube_blas_desc.name = "Cube BLAS";
        m_cube_blas = g_render_device->create_acceleration_structure(cube_blas_desc);

        const auto instances_geo = AccelerationStructureGeometry::instances(2, true);

        AccelerationStructureDescription cube_tlas_desc{};
        cube_tlas_desc.type = AccelerationStructureType::TopLevel;
        cube_tlas_desc.geometry = {instances_geo};
        cube_tlas_desc.name = "Cube TLAS";
        m_cube_tlas = g_render_device->create_acceleration_structure(cube_tlas_desc);

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

        m_point_lights.push_back(
            RtxPointLight{
                .position = glm::vec3(2.0f, 3.0f, 0.0f),
                .radius = 1.0f,
                .color = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f),
            });

        m_point_lights.push_back(
            RtxPointLight{
                .position = glm::vec3(-2.0f, 3.0f, 0.0f),
                .radius = 1.0f,
                .color = glm::vec4(0.1f, 0.3f, 0.8f, 1.0f),
            });
    }
};

class SimpleRtxGameMain : public GameMain
{
  public:
    GameDescription get_game_description() const
    {
        constexpr GraphicsApi graphics_api = GraphicsApi::Vulkan;
        static_assert(graphics_api == GraphicsApi::Vulkan, "SimpleRtx example only supports Vulkan backend.");

        GameDescription desc{};
        desc.name = "SimpleRtx Example";
        desc.version = Version{0, 1, 0};
        desc.graphics_api = graphics_api;
        desc.width = 1920;
        desc.height = 1080;

        return desc;
    }

    GameSimulation* create_game_simulation() const { return new SimpleRtxSimulation{}; }

    void setup_game_renderer(GameRenderer& game_renderer) const
    {
        game_renderer.register_module<SimpleRtxRenderModule>(RenderModuleLabel::Scene);
    }
};

GameMain* Mizu::create_game_main()
{
    return new SimpleRtxGameMain{};
}
