#pragma once

#include <array>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "base/debug/assert.h"
#include "render/frame_linear_allocator.h"
#include "render/systems/pipeline_cache.h"
#include "render/utils/buffer_utils.h"
#include "render/utils/command_utils.h"
#include "render_core/rhi/acceleration_structure.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/descriptors.h"
#include "render_core/rhi/pipeline.h"

#include "render_tests.pipeline/simple_rtx_shaders.h"
#include "runner/render_test.h"
#include "runner/render_tests_registry.h"

using namespace Mizu;

struct CameraInfoUBO
{
    glm::mat4 viewProj;
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
};

struct RtxPointLight
{
    glm::vec3 position;
    float radius;
    glm::vec4 color;
};

struct RtxVertex
{
    glm::vec3 position;
    float pad0;
    glm::vec3 normal;
    float pad1;
};

class SimpleRtxRenderTest : public RenderTest
{
  public:
    std::string_view get_test_group_name() const override { return "Basic"; }
    std::string_view get_test_name() const override { return "SimpleRtx"; }

    bool should_run_test(const RenderTestEnvironment& environment, const DeviceProperties& device_props) const override
    {
        // Hardware rtx only supported in Vulkan for the moment
        return device_props.ray_tracing_hardware && environment.graphics_api == GraphicsApi::Vulkan;
    }

    void prepare_test(const RenderTestExecutionEnvironment& environment) override
    {
        create_cube_mesh();
        build_acceleration_structures();
        create_point_lights();
        create_camera_info(environment);
    }

    void cleanup_test() override
    {
        m_cube_vb = nullptr;
        m_cube_ib = nullptr;
        m_cube_blas = nullptr;
        m_cube_tlas = nullptr;
        m_as_scratch_buffer = nullptr;
        m_point_lights_buffer = nullptr;
        m_camera_info = nullptr;
    }

    void run_test(RenderGraphBuilder& builder, const RenderTestExecutionEnvironment& environment) override
    {
        const uint32_t width = environment.output_width;
        const uint32_t height = environment.output_height;

        const RenderGraphResource cube_tlas_ref = builder.register_external_acceleration_structure(
            m_cube_tlas,
            {AccelerationStructureResourceState::AccelStructRead, AccelerationStructureResourceState::AccelStructRead});
        const RenderGraphResource scratch_buffer_ref = builder.register_external_buffer(m_as_scratch_buffer, {});

        struct BuildAsData
        {
            RenderGraphResource tlas;
            RenderGraphResource scratch_buffer;
        };

        builder.add_pass<BuildAsData>(
            "BuildTlas",
            [&](RenderGraphPassBuilder& pass, BuildAsData& data) {
                pass.set_hint(RenderGraphPassHint::Transfer);

                data.tlas = pass.write(cube_tlas_ref);
                data.scratch_buffer = pass.accel_struct_scratch(scratch_buffer_ref);
            },
            [=, this](CommandBuffer& command, const BuildAsData& data, const RenderGraphPassResources& resources) {
                glm::mat4 cube_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
                cube_transform = glm::scale(cube_transform, glm::vec3(0.5f));

                glm::mat4 floor_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
                floor_transform = glm::scale(floor_transform, glm::vec3(2.0f, 0.075f, 2.0f));

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

                const auto tlas = resources.get_acceleration_structure(data.tlas);
                const auto scratch_buffer = resources.get_buffer(data.scratch_buffer);

                command.update_tlas(*tlas, instances, *scratch_buffer);
            });

        const RenderGraphResource camera_info_ref = builder.register_external_buffer(
            m_camera_info, {BufferResourceState::ShaderReadOnly, BufferResourceState::ShaderReadOnly});

        const RenderGraphResource vertices_ref = builder.register_external_buffer(
            m_cube_vb, {BufferResourceState::ShaderReadOnly, BufferResourceState::ShaderReadOnly});
        const RenderGraphResource indices_ref = builder.register_external_buffer(
            m_cube_ib, {BufferResourceState::ShaderReadOnly, BufferResourceState::ShaderReadOnly});

        const RenderGraphResource point_lights_ref = builder.register_external_buffer(
            m_point_lights_buffer, {BufferResourceState::ShaderReadOnly, BufferResourceState::ShaderReadOnly});

        struct TraceRaysData
        {
            RenderGraphResource camera_info;
            RenderGraphResource output_texture;
            RenderGraphResource scene_tlas;
            RenderGraphResource vertices;
            RenderGraphResource indices;
            RenderGraphResource point_lights;
        };

        builder.add_pass<TraceRaysData>(
            "TraceRays",
            [&](RenderGraphPassBuilder& pass, TraceRaysData& data) {
                pass.set_hint(RenderGraphPassHint::RayTracing);

                data.camera_info = pass.read(camera_info_ref);
                data.output_texture = pass.write(environment.output_texture);
                data.scene_tlas = pass.read(cube_tlas_ref);
                data.vertices = pass.read(vertices_ref);
                data.indices = pass.read(indices_ref);
                data.point_lights = pass.read(point_lights_ref);
            },
            [=](CommandBuffer& command, const TraceRaysData& data, const RenderGraphPassResources& resources) {
                const auto pipeline = get_ray_tracing_pipeline(
                    SimpleRtxRaygen{}.get_instance(),
                    {SimpleRtxMiss{}.get_instance(), SimpleRtxShadowMiss{}.get_instance()},
                    {SimpleRtxClosestHit{}.get_instance()},
                    1);
                command.bind_pipeline(pipeline);

                // clang-format off
                MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(TraceRaysLayout0)
                    MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::RtxRaygen)
                    MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_UAV(0, 1, ShaderType::RtxRaygen)
                    MIZU_DESCRIPTOR_SET_LAYOUT_ACCELERATION_STRUCTURE(0, 1, ShaderType::RtxRaygen | ShaderType::RtxClosestHit)
                MIZU_END_DESCRIPTOR_SET_LAYOUT()
                // clang-format on

                std::array writes_0 = {
                    WriteDescriptor::ConstantBuffer(
                        0, BufferResourceView::create(resources.get_buffer(data.camera_info))),
                    WriteDescriptor::TextureUav(0, ImageResourceView::create(resources.get_image(data.output_texture))),
                    WriteDescriptor::AccelerationStructure(
                        0, AccelerationStructureView::create(resources.get_acceleration_structure(data.scene_tlas))),
                };

                const auto descriptor_set_0 = g_render_device->allocate_descriptor_set(
                    TraceRaysLayout0::get_layout(), DescriptorSetAllocationType::Transient);
                descriptor_set_0->update(writes_0);

                // clang-format off
                MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(TraceRaysLayout1)
                    MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::RtxClosestHit)
                    MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::RtxClosestHit)
                    MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(2, 1, ShaderType::RtxClosestHit)
                MIZU_END_DESCRIPTOR_SET_LAYOUT()
                // clang-format on

                std::array writes_1 = {
                    WriteDescriptor::StructuredBufferSrv(
                        0, BufferResourceView::create(resources.get_buffer(data.vertices))),
                    WriteDescriptor::StructuredBufferSrv(
                        1, BufferResourceView::create(resources.get_buffer(data.indices))),
                    WriteDescriptor::StructuredBufferSrv(
                        2, BufferResourceView::create(resources.get_buffer(data.point_lights))),
                };

                const auto descriptor_set_1 = g_render_device->allocate_descriptor_set(
                    TraceRaysLayout1::get_layout(), DescriptorSetAllocationType::Transient);
                descriptor_set_1->update(writes_1);

                command.bind_descriptor_set(descriptor_set_0, 0);
                command.bind_descriptor_set(descriptor_set_1, 1);

                command.trace_rays({width, height, 1});
            });
    }

  private:
    std::shared_ptr<BufferResource> m_cube_vb;
    std::shared_ptr<BufferResource> m_cube_ib;
    std::shared_ptr<AccelerationStructure> m_cube_blas;
    std::shared_ptr<AccelerationStructure> m_cube_tlas;
    std::shared_ptr<BufferResource> m_as_scratch_buffer;
    std::shared_ptr<BufferResource> m_point_lights_buffer;
    std::shared_ptr<BufferResource> m_camera_info;
    std::vector<RtxPointLight> m_point_lights;

    void create_cube_mesh()
    {
        // TODO: Evaluate uploading mesh data via FrameLinearAllocator in a render graph transfer pass and issuing
        // build_blas/build_tlas inside the graph instead of using CommandUtils::submit_single_time +
        // BufferUtils::initialize_buffer.

        // clang-format off
        const std::vector<RtxVertex> vertices = {
            // Front face (normal: 0,0,-1)
            {{-1.0f, -1.0f, -1.0f}, 0, {0.0f, 0.0f, -1.0f}, 0},
            {{ 1.0f, -1.0f, -1.0f}, 0, {0.0f, 0.0f, -1.0f}, 0},
            {{ 1.0f,  1.0f, -1.0f}, 0, {0.0f, 0.0f, -1.0f}, 0},
            {{-1.0f,  1.0f, -1.0f}, 0, {0.0f, 0.0f, -1.0f}, 0},

            // Back face (normal: 0,0,1)
            {{ 1.0f, -1.0f,  1.0f}, 0, {0.0f, 0.0f,  1.0f}, 0},
            {{-1.0f, -1.0f,  1.0f}, 0, {0.0f, 0.0f,  1.0f}, 0},
            {{-1.0f,  1.0f,  1.0f}, 0, {0.0f, 0.0f,  1.0f}, 0},
            {{ 1.0f,  1.0f,  1.0f}, 0, {0.0f, 0.0f,  1.0f}, 0},

            // Left face (normal: -1,0,0)
            {{-1.0f, -1.0f,  1.0f}, 0, {-1.0f, 0.0f, 0.0f}, 0},
            {{-1.0f, -1.0f, -1.0f}, 0, {-1.0f, 0.0f, 0.0f}, 0},
            {{-1.0f,  1.0f, -1.0f}, 0, {-1.0f, 0.0f, 0.0f}, 0},
            {{-1.0f,  1.0f,  1.0f}, 0, {-1.0f, 0.0f, 0.0f}, 0},

            // Right face (normal: 1,0,0)
            {{ 1.0f, -1.0f, -1.0f}, 0, {1.0f, 0.0f, 0.0f}, 0},
            {{ 1.0f, -1.0f,  1.0f}, 0, {1.0f, 0.0f, 0.0f}, 0},
            {{ 1.0f,  1.0f,  1.0f}, 0, {1.0f, 0.0f, 0.0f}, 0},
            {{ 1.0f,  1.0f, -1.0f}, 0, {1.0f, 0.0f, 0.0f}, 0},

            // Top face (normal: 0,1,0)
            {{-1.0f,  1.0f, -1.0f}, 0, {0.0f, 1.0f, 0.0f}, 0},
            {{ 1.0f,  1.0f, -1.0f}, 0, {0.0f, 1.0f, 0.0f}, 0},
            {{ 1.0f,  1.0f,  1.0f}, 0, {0.0f, 1.0f, 0.0f}, 0},
            {{-1.0f,  1.0f,  1.0f}, 0, {0.0f, 1.0f, 0.0f}, 0},

            // Bottom face (normal: 0,-1,0)
            {{-1.0f, -1.0f,  1.0f}, 0, {0.0f, -1.0f, 0.0f}, 0},
            {{ 1.0f, -1.0f,  1.0f}, 0, {0.0f, -1.0f, 0.0f}, 0},
            {{ 1.0f, -1.0f, -1.0f}, 0, {0.0f, -1.0f, 0.0f}, 0},
            {{-1.0f, -1.0f, -1.0f}, 0, {0.0f, -1.0f, 0.0f}, 0},
        };

        const std::vector<uint32_t> indices = {
            // Front
            0, 1, 2,  0, 2, 3,
            // Back
            4, 5, 6,  4, 6, 7,
            // Left
            8, 9, 10, 8, 10, 11,
            // Right
            12, 13, 14, 12, 14, 15,
            // Top
            16, 17, 18, 16, 18, 19,
            // Bottom
            20, 21, 22, 20, 22, 23,
        };
        // clang-format on

        BufferDescription vb_desc{};
        vb_desc.size = sizeof(RtxVertex) * vertices.size();
        vb_desc.usage = BufferUsageBits::VertexBuffer | BufferUsageBits::TransferDst | BufferUsageBits::ShaderResource
                        | BufferUsageBits::RtxAccelerationStructureInputReadOnly;
        vb_desc.name = "Cube VertexBuffer";

        BufferDescription ib_desc{};
        ib_desc.size = sizeof(uint32_t) * indices.size();
        ib_desc.usage = BufferUsageBits::IndexBuffer | BufferUsageBits::TransferDst | BufferUsageBits::ShaderResource
                        | BufferUsageBits::RtxAccelerationStructureInputReadOnly;
        ib_desc.name = "Cube IndexBuffer";

        m_cube_vb = g_render_device->create_buffer(vb_desc);
        m_cube_ib = g_render_device->create_buffer(ib_desc);

        BufferUtils::initialize_buffer(*m_cube_vb, reinterpret_cast<const uint8_t*>(vertices.data()), vb_desc.size);
        BufferUtils::initialize_buffer(*m_cube_ib, reinterpret_cast<const uint8_t*>(indices.data()), ib_desc.size);
    }

    void build_acceleration_structures()
    {
        const auto triangles_geo = AccelerationStructureGeometry::triangles(
            m_cube_vb, ImageFormat::R32G32B32_SFLOAT, sizeof(RtxVertex), m_cube_ib);

        AccelerationStructureDescription blas_desc{};
        blas_desc.type = AccelerationStructureType::BottomLevel;
        blas_desc.geometry = {triangles_geo};
        blas_desc.name = "Cube BLAS";
        m_cube_blas = g_render_device->create_acceleration_structure(blas_desc);

        const auto instances_geo = AccelerationStructureGeometry::instances(2, true);

        AccelerationStructureDescription tlas_desc{};
        tlas_desc.type = AccelerationStructureType::TopLevel;
        tlas_desc.geometry = {instances_geo};
        tlas_desc.name = "Cube TLAS";
        m_cube_tlas = g_render_device->create_acceleration_structure(tlas_desc);

        BufferDescription scratch_desc{};
        scratch_desc.size = glm::max(
            m_cube_blas->get_build_sizes().build_scratch_size, m_cube_tlas->get_build_sizes().build_scratch_size);
        scratch_desc.usage = BufferUsageBits::RtxAccelerationStructureStorage | BufferUsageBits::UnorderedAccess;
        m_as_scratch_buffer = g_render_device->create_buffer(scratch_desc);

        CommandUtils::submit_single_time(CommandBufferType::Graphics, [=, this](CommandBuffer& command) {
            command.build_blas(*m_cube_blas, *m_as_scratch_buffer);
        });

        CommandUtils::submit_single_time(CommandBufferType::Graphics, [=, this](CommandBuffer& command) {
            glm::mat4 cube_transform = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));

            glm::mat4 floor_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
            floor_transform = glm::scale(floor_transform, glm::vec3(2.0f, 0.075f, 2.0f));

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
    }

    void create_point_lights()
    {
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

        BufferDescription lights_desc{};
        lights_desc.size = sizeof(RtxPointLight) * m_point_lights.size();
        lights_desc.stride = 0;
        lights_desc.usage = BufferUsageBits::HostVisible | BufferUsageBits::ShaderResource;
        lights_desc.name = "PointLights";
        m_point_lights_buffer = g_render_device->create_buffer(lights_desc);
        m_point_lights_buffer->set_data(reinterpret_cast<const uint8_t*>(m_point_lights.data()));
    }

    void create_camera_info(const RenderTestExecutionEnvironment& environment)
    {
        const float aspect =
            static_cast<float>(environment.output_width) / static_cast<float>(environment.output_height);

        glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.001f, 100.0f);
        proj[1][1] *= -1.0f;

        glm::mat4 view =
            glm::lookAt(glm::vec3(0.0f, 0.5f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        CameraInfoUBO camera_info{};
        camera_info.viewProj = proj * view;
        camera_info.viewInverse = glm::inverse(view);
        camera_info.projInverse = glm::inverse(proj);

        m_camera_info = BufferUtils::create_constant_buffer(camera_info, "CameraInfo");
    }
};

REGISTER_RENDER_TEST(SimpleRtxRenderTest);
