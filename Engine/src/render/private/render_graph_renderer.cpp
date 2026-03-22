#include "render/render_graph_renderer.h"

#include <format>
#include <glm/gtc/matrix_transform.hpp>

#include "base/debug/profiling.h"
#include "core/thread_sync.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/sampler_state.h"

#include "render.pipeline/render_graph_renderer_shaders.h"
#include "render/core/camera.h"
#include "render/material/material.h"
#include "render/passes/pass_info.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/render_graph/render_graph_utils.h"
#include "render/runtime/renderer.h"
#include "render/state_manager/camera_state_manager.h"
#include "render/state_manager/light_state_manager.h"
#include "render/state_manager/renderer_settings_state_manager.h"
#include "render/state_manager/static_mesh_state_manager.h"
#include "render/state_manager/transform_state_manager.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "render/utils/buffer_utils.h"
#include "render/utils/render_utils.h"
#include "render_graph_renderer_parameters.h"

namespace Mizu
{

struct GPUCameraInfo
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewProj;
    glm::mat4 inverseView;
    glm::mat4 inverseProj;
    glm::mat4 inverseViewProj;
    glm::vec3 pos;

    float _pad0;

    float znear;
    float zfar;

    glm::vec2 _pad1;
};

struct GpuLightCullingInfo
{
    glm::uvec2 num_tiles;
};

struct RenderGraphRendererFrameInfo
{
    uint32_t width, height;
    GPUCameraInfo camera_info;
    FrameAllocation camera_info_view;
    RenderGraphResource output_texture;
};

struct DrawInfo
{
    DrawListHandle main_view_handle;
    DrawListHandle shadows_view_handle;

    FrameAllocation transform_info_view;
    FrameAllocation main_view_indices_view;
    FrameAllocation shadows_indices_view;
};

struct LightsInfo
{
    FrameAllocation point_lights_view;
    FrameAllocation directional_lights_view;
    FrameAllocation cascade_light_space_matrices_view;
};

struct DepthNormalsPrepassInfo
{
    RenderGraphResource depth_texture;
};

struct LightCullingInfo
{
    RenderGraphResource visible_point_light_indices;
    FrameAllocation light_culling_info;
};

struct ShadowsInfo
{
    RenderGraphResource shadow_map_texture;
    FrameAllocation cascade_splits_view;
};

RenderGraphRenderer::RenderGraphRenderer()
{
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
    fullscreen_triangle_desc.stride = static_cast<uint32_t>(sizeof(FullscreenTriangleVertex));
    fullscreen_triangle_desc.usage = BufferUsageBits::VertexBuffer | BufferUsageBits::TransferDst;
    fullscreen_triangle_desc.name = "Fullscreen Triangle";

    m_fullscreen_triangle = g_render_device->create_buffer(fullscreen_triangle_desc);
    BufferUtils::initialize_buffer(
        *m_fullscreen_triangle, reinterpret_cast<const uint8_t*>(vertices.data()), fullscreen_triangle_desc.size);

    m_draw_manager = std::make_unique<DrawBlockManager>();

    m_transform_info_buffer.resize(TRANSFORM_INFO_BUFFER_SIZE);
    m_main_view_transform_indices_buffer.resize(TRANSFORM_INFO_BUFFER_SIZE);
    m_shadows_view_transform_indices_buffer.resize(TRANSFORM_INFO_BUFFER_SIZE);
}

void RenderGraphRenderer::build_render_graph2(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard)
{
    MIZU_PROFILE_SCOPED;

    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const Camera& camera = rend_get_camera_state();

    RenderGraphRendererSettings& settings = blackboard.add<RenderGraphRendererSettings>();
    settings = rend_get_renderer_settings().settings;

    GPUCameraInfo gpu_camera_info{};
    gpu_camera_info.view = camera.get_view_matrix();
    gpu_camera_info.proj = camera.get_projection_matrix();
    gpu_camera_info.viewProj = gpu_camera_info.proj * gpu_camera_info.view;
    gpu_camera_info.inverseView = glm::inverse(gpu_camera_info.view);
    gpu_camera_info.inverseProj = glm::inverse(gpu_camera_info.proj);
    gpu_camera_info.inverseViewProj = glm::inverse(gpu_camera_info.viewProj);
    gpu_camera_info.pos = camera.get_position();
    gpu_camera_info.znear = camera.get_znear();
    gpu_camera_info.zfar = camera.get_zfar();

    const FrameAllocation camera_info = frame_info.frame_allocator->allocate_constant<GPUCameraInfo>();
    camera_info.upload(gpu_camera_info);

    RenderGraphRendererFrameInfo& rgr_frame_info = blackboard.add<RenderGraphRendererFrameInfo>();
    rgr_frame_info.width = frame_info.width;
    rgr_frame_info.height = frame_info.height;
    rgr_frame_info.camera_info = gpu_camera_info;
    rgr_frame_info.camera_info_view = camera_info;
    rgr_frame_info.output_texture = frame_info.output_texture_ref;

    m_draw_manager->reset();

    get_light_information(blackboard);
    create_draw_lists(blackboard);

    render_scene(builder, blackboard);
}

void RenderGraphRenderer::render_scene(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard) const
{
    add_depth_normals_prepass(builder, blackboard);
    add_light_culling_pass(builder, blackboard);
    add_cascaded_shadow_mapping_pass(builder, blackboard);
    add_lighting_pass(builder, blackboard);

    const DebugSettings& debug_settings = blackboard.get<RenderGraphRendererSettings>().debug;
    if (debug_settings.view == DebugSettings::DebugView::LightCulling)
        add_light_culling_debug_pass(builder, blackboard);
    else if (debug_settings.view == DebugSettings::DebugView::CascadedShadows)
        add_cascaded_shadow_mapping_debug_pass(builder, blackboard);
}

void RenderGraphRenderer::add_depth_normals_prepass(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard)
    const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    const DrawInfo& draw_info = blackboard.get<DrawInfo>();

    const RenderGraphResource depth_texture =
        builder.create_texture2d(frame_info.width, frame_info.height, ImageFormat::D32_SFLOAT, "DepthTexture");

    struct DepthNormalsData
    {
        RenderGraphResource depth_texture;
    };

    builder.add_pass<DepthNormalsData>(
        "DepthNormalsPrepass",
        [&](RenderGraphPassBuilder2& pass, DepthNormalsData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);
            data.depth_texture = pass.attachment(depth_texture);
        },
        [=, this](CommandBuffer& command, const DepthNormalsData& data, const RenderGraphPassResources2& resources) {
            FramebufferAttachment2 depth_attachment{};
            depth_attachment.rtv = ImageResourceView::create(resources.get_image(data.depth_texture));
            depth_attachment.load_operation = LoadOperation::Clear;
            depth_attachment.store_operation = StoreOperation::Store;
            depth_attachment.clear_value = glm::vec4(1.0f);

            RenderPassInfo2 pass_info{};
            pass_info.extent = {frame_info.width, frame_info.height};
            pass_info.depth_stencil_attachment = depth_attachment;

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(DepthNormalsLayout)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Vertex)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Vertex)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Vertex)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::ConstantBuffer(0, frame_info.camera_info_view.view),
                WriteDescriptor::StructuredBufferSrv(0, draw_info.transform_info_view.view),
                WriteDescriptor::StructuredBufferSrv(1, draw_info.main_view_indices_view.view),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                DepthNormalsLayout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            DepthStencilState depth_stencil{};
            depth_stencil.depth_test = true;
            depth_stencil.depth_write = true;

            FramebufferInfo framebuffer_info{};
            framebuffer_info.depth_stencil_attachment = resources.get_image(data.depth_texture)->get_format();

            command.begin_render_pass(pass_info);
            {
                const auto pipeline = get_graphics_pipeline(
                    DepthNormalsPrepassShaderVS{},
                    DepthNormalsPrepassShaderFS{},
                    RasterizationState{},
                    depth_stencil,
                    ColorBlendState{},
                    framebuffer_info);
                command.bind_pipeline(pipeline);

                command.bind_descriptor_set(descriptor_set, 0);

                const DrawList& list = m_draw_manager->get_draw_list(draw_info.main_view_handle);
                for (size_t block_idx = 0; block_idx < list.num_blocks; ++block_idx)
                {
                    const DrawBlock& block = list.blocks[block_idx];

                    const std::string draw_block_name = std::format("DrawBlock:{}", block.pipeline_hash);
                    command.begin_gpu_marker(draw_block_name);

                    for (size_t elem_idx = 0; elem_idx < block.num_elements; ++elem_idx)
                    {
                        const DrawElement& element = block.elements[elem_idx];

                        struct PushConstant
                        {
                            uint64_t transform_offset;
                        } push_constant{};

                        push_constant.transform_offset = element.transform_offset;
                        command.push_constant(push_constant);

                        draw_mesh_instanced(command, *element.mesh, element.instance_count);
                    }

                    command.end_gpu_marker();
                }
            }
            command.end_render_pass();
        });

    DepthNormalsPrepassInfo& depth_normals_prepass_info = blackboard.add<DepthNormalsPrepassInfo>();
    depth_normals_prepass_info.depth_texture = depth_texture;
}

void RenderGraphRenderer::add_light_culling_pass(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    const LightsInfo& lights_info = blackboard.get<LightsInfo>();
    const DepthNormalsPrepassInfo& depth_normals_info = blackboard.get<DepthNormalsPrepassInfo>();
    FrameLinearAllocator& frame_allocator = *blackboard.get<FrameInfo>().frame_allocator;

    // Should match values defined in LightCullingCommon.slang
    constexpr uint32_t TILE_SIZE = LightCullingShaderCS::TILE_SIZE;
    constexpr uint32_t MAX_LIGHTS_PER_TILE = LightCullingShaderCS::MAX_LIGHTS_PER_TILE;

    const glm::uvec3 group_count =
        compute_group_count({frame_info.width, frame_info.height, 1.0f}, {TILE_SIZE, TILE_SIZE, 1.0f});

    const uint32_t num_tiles = group_count.x * group_count.y;
    const uint32_t point_lights_number = num_tiles * MAX_LIGHTS_PER_TILE;

    const RenderGraphResource visible_point_light_indices =
        builder.create_structured_buffer<uint32_t>(point_lights_number, "VisiblePointLightIndices");

    GpuLightCullingInfo gpu_light_culling_info{};
    gpu_light_culling_info.num_tiles = glm::uvec2(group_count);

    const FrameAllocation light_culling_info = frame_allocator.allocate_constant<GpuLightCullingInfo>();
    light_culling_info.upload(gpu_light_culling_info);

    struct LightCullingData
    {
        RenderGraphResource visible_point_light_indices;
        RenderGraphResource depth_texture;
        FrameAllocation light_culling_info;
    };

    builder.add_pass<LightCullingData>(
        "LightCulling",
        [&](RenderGraphPassBuilder2& pass, LightCullingData& data) {
            pass.set_hint(RenderGraphPassHint::Compute);

            data.visible_point_light_indices = pass.write(visible_point_light_indices);
            data.depth_texture = pass.read(depth_normals_info.depth_texture);
            data.light_culling_info = light_culling_info;
        },
        [=](CommandBuffer& command, const LightCullingData& data, const RenderGraphPassResources2& resources) {
            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightCullingLayout_0)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Compute)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()

            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightCullingLayout_1)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Compute)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(0, 1, ShaderType::Compute)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Compute)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()

            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightCullingLayout_2)
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(0, 1, ShaderType::Compute)
                MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(0, 1, ShaderType::Compute)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            std::array writes_0 = {
                WriteDescriptor::ConstantBuffer(0, frame_info.camera_info_view.view),
            };

            std::array writes_1 = {
                WriteDescriptor::StructuredBufferSrv(0, lights_info.point_lights_view.view),
                WriteDescriptor::StructuredBufferUav(
                    0, BufferResourceView::create(resources.get_buffer(data.visible_point_light_indices))),
                WriteDescriptor::ConstantBuffer(0, data.light_culling_info.view),
            };

            std::array writes_2 = {
                WriteDescriptor::TextureSrv(0, ImageResourceView::create(resources.get_image(data.depth_texture))),
                WriteDescriptor::SamplerState(0, get_sampler_state({})),
            };

            const auto descriptor_set_0 = g_render_device->allocate_descriptor_set(
                LightCullingLayout_0::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_0->update(writes_0);

            const auto descriptor_set_1 = g_render_device->allocate_descriptor_set(
                LightCullingLayout_1::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_1->update(writes_1);

            const auto descriptor_set_2 = g_render_device->allocate_descriptor_set(
                LightCullingLayout_2::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_2->update(writes_2);

            const auto pipeline = get_compute_pipeline(LightCullingShaderCS{});
            command.bind_pipeline(pipeline);

            command.bind_descriptor_set(descriptor_set_0, 0);
            command.bind_descriptor_set(descriptor_set_1, 1);
            command.bind_descriptor_set(descriptor_set_2, 2);

            command.dispatch(group_count);
        });

    LightCullingInfo& culling_info = blackboard.add<LightCullingInfo>();
    culling_info.visible_point_light_indices = visible_point_light_indices;
    culling_info.light_culling_info = light_culling_info;
}

void RenderGraphRenderer::add_cascaded_shadow_mapping_pass(
    RenderGraphBuilder2& builder,
    RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const CascadedShadowsSettings& shadow_settings = blackboard.get<RenderGraphRendererSettings>().cascaded_shadows;
    const GPUCameraInfo& camera_info = blackboard.get<RenderGraphRendererFrameInfo>().camera_info;
    const LightsInfo& lights_info = blackboard.get<LightsInfo>();
    const DrawInfo& draw_info = blackboard.get<DrawInfo>();
    FrameLinearAllocator& frame_allocator = *blackboard.get<FrameInfo>().frame_allocator;

    const uint32_t num_shadow_casting_directional_lights =
        static_cast<uint32_t>(m_cascade_light_space_matrices.size()) / shadow_settings.num_cascades;

    inplace_vector<float, CascadedShadowsSettings::MAX_NUM_CASCADES> cascade_splits_buffer;
    for (uint32_t cascade_idx = 0; cascade_idx < shadow_settings.num_cascades; ++cascade_idx)
    {
        const float clip_range = camera_info.zfar - camera_info.znear;
        const float cascade_split =
            (camera_info.znear + shadow_settings.cascade_split_factors[cascade_idx] * clip_range) * -1.0f;
        cascade_splits_buffer.push_back(cascade_split);
    }

    const FrameAllocation cascade_splits = frame_allocator.allocate_structured<float>(shadow_settings.num_cascades);
    cascade_splits.upload(cascade_splits_buffer);

    const uint32_t width = std::max(shadow_settings.resolution * shadow_settings.num_cascades, 1u);
    const uint32_t height = std::max(shadow_settings.resolution * num_shadow_casting_directional_lights, 1u);

    const RenderGraphResource shadow_map_texture =
        builder.create_texture2d(width, height, ImageFormat::D32_SFLOAT, "ShadowMapTexture");

    struct CascadedShadowMappingData
    {
        RenderGraphResource shadow_map_texture;
    };

    builder.add_pass<CascadedShadowMappingData>(
        "CascadedShadowMapping",
        [&](RenderGraphPassBuilder2& pass, CascadedShadowMappingData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);
            data.shadow_map_texture = pass.attachment(shadow_map_texture);
        },
        [=, this](
            CommandBuffer& command, const CascadedShadowMappingData& data, const RenderGraphPassResources2& resources) {
            FramebufferAttachment2 depth_attachment{};
            depth_attachment.rtv = ImageResourceView::create(resources.get_image(data.shadow_map_texture));
            depth_attachment.load_operation = LoadOperation::Clear;
            depth_attachment.store_operation = StoreOperation::Store;
            depth_attachment.clear_value = glm::vec4(1.0f);

            RenderPassInfo2 pass_info{};
            pass_info.extent = {width, height};
            pass_info.depth_stencil_attachment = depth_attachment;

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(CascadedShadowMappingLayout_0)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Vertex) 
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Vertex)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(2, 1, ShaderType::Vertex) 
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes_0 = {
                WriteDescriptor::StructuredBufferSrv(0, lights_info.cascade_light_space_matrices_view.view),
                WriteDescriptor::StructuredBufferSrv(1, draw_info.transform_info_view.view),
                WriteDescriptor::StructuredBufferSrv(2, draw_info.shadows_indices_view.view),
            };

            const auto descriptor_set_0 = g_render_device->allocate_descriptor_set(
                CascadedShadowMappingLayout_0::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_0->update(writes_0);

            RasterizationState raster{};
            raster.depth_clamp = true;
            raster.cull_mode = RasterizationState::CullMode::Front;

            DepthStencilState depth_stencil{};
            depth_stencil.depth_test = true;
            depth_stencil.depth_write = true;

            FramebufferInfo framebuffer_info{};
            framebuffer_info.depth_stencil_attachment = ImageFormat::D32_SFLOAT;

            command.begin_render_pass(pass_info);
            {
                const auto pipeline = get_graphics_pipeline(
                    CascadedShadowMappingShaderVS{},
                    CascadedShadowMappingShaderFS{},
                    raster,
                    depth_stencil,
                    ColorBlendState{},
                    framebuffer_info);
                command.bind_pipeline(pipeline);

                command.bind_descriptor_set(descriptor_set_0, 0);

                struct PushConstant
                {
                    uint32_t num_cascades;
                    uint32_t num_lights;
                    uint64_t transform_offset;
                } push_constant{};

                push_constant.num_cascades = shadow_settings.num_cascades;
                push_constant.num_lights = num_shadow_casting_directional_lights;

                const DrawList& list = m_draw_manager->get_draw_list(draw_info.shadows_view_handle);

                for (size_t block_idx = 0; block_idx < list.num_blocks; ++block_idx)
                {
                    const DrawBlock& block = list.blocks[block_idx];

                    const std::string draw_block_name = std::format("DrawBlock:{}", block.pipeline_hash);
                    command.begin_gpu_marker(draw_block_name);

                    for (size_t elem_idx = 0; elem_idx < block.num_elements; ++elem_idx)
                    {
                        const DrawElement& element = block.elements[elem_idx];

                        push_constant.transform_offset = element.transform_offset;
                        command.push_constant(push_constant);

                        const uint32_t num_instances =
                            shadow_settings.num_cascades * push_constant.num_lights * element.instance_count;
                        draw_mesh_instanced(command, *element.mesh, num_instances);
                    }

                    command.end_gpu_marker();
                }
            }
            command.end_render_pass();
        });

    ShadowsInfo& shadows_info = blackboard.add<ShadowsInfo>();
    shadows_info.shadow_map_texture = shadow_map_texture;
    shadows_info.cascade_splits_view = cascade_splits;
}

void RenderGraphRenderer::add_lighting_pass(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    const DrawInfo& draw_info = blackboard.get<DrawInfo>();
    const LightsInfo& lights_info = blackboard.get<LightsInfo>();
    const DepthNormalsPrepassInfo& depth_normals_info = blackboard.get<DepthNormalsPrepassInfo>();
    const LightCullingInfo& culling_info = blackboard.get<LightCullingInfo>();
    const ShadowsInfo& shadows_info = blackboard.get<ShadowsInfo>();

    const auto directional_shadow_map_sampler = get_sampler_state(
        SamplerStateDescription{
            .address_mode_u = ImageAddressMode::ClampToEdge,
            .address_mode_v = ImageAddressMode::ClampToEdge,
            .address_mode_w = ImageAddressMode::ClampToEdge,
            .border_color = BorderColor::FloatOpaqueWhite,
        });

    struct LightingData
    {
        RenderGraphResource output_texture;
        RenderGraphResource depth_texture;
        RenderGraphResource visible_point_light_indices;
        RenderGraphResource directional_shadow_map;
    };

    GraphicsPipelineDescription pipeline_desc{};
    pipeline_desc.depth_stencil.depth_test = true;
    pipeline_desc.depth_stencil.depth_write = false;
    pipeline_desc.depth_stencil.depth_compare_op = DepthStencilState::DepthCompareOp::LessEqual;

    builder.add_pass<LightingData>(
        "Lighting",
        [&](RenderGraphPassBuilder2& pass, LightingData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);

            data.output_texture = pass.attachment(frame_info.output_texture);
            data.depth_texture = pass.attachment(depth_normals_info.depth_texture);
            data.visible_point_light_indices = pass.read(culling_info.visible_point_light_indices);
            data.directional_shadow_map = pass.read(shadows_info.shadow_map_texture);
        },
        [=, this](CommandBuffer& command, const LightingData& data, const RenderGraphPassResources2& resources) {
            ImageResourceViewDescription output_view_desc{};
            output_view_desc.override_format = ImageFormat::R8G8B8A8_SRGB;

            FramebufferAttachment2 color_attachment{};
            color_attachment.rtv =
                ImageResourceView::create(resources.get_image(data.output_texture), output_view_desc);
            color_attachment.load_operation = LoadOperation::Clear;
            color_attachment.store_operation = StoreOperation::Store;
            color_attachment.clear_value = glm::vec4(0.0f);

            FramebufferAttachment2 depth_attachment{};
            depth_attachment.rtv = ImageResourceView::create(resources.get_image(data.depth_texture));
            depth_attachment.load_operation = LoadOperation::Load;
            depth_attachment.store_operation = StoreOperation::Store;

            RenderPassInfo2 pass_info{};
            pass_info.extent = {frame_info.width, frame_info.height};
            pass_info.color_attachments = {color_attachment};
            pass_info.depth_stencil_attachment = depth_attachment;

            FramebufferInfo framebuffer_info{};
            framebuffer_info.color_attachments = {*output_view_desc.override_format};
            framebuffer_info.depth_stencil_attachment = resources.get_image(data.depth_texture)->get_format();

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightingLayout_0)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Vertex | ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Vertex)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Vertex)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()

            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightingLayout_1)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(2, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(3, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(0, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(4, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(5, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(1, 1, ShaderType::Fragment)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes_0 = {
                WriteDescriptor::ConstantBuffer(0, frame_info.camera_info_view.view),
                WriteDescriptor::StructuredBufferSrv(0, draw_info.transform_info_view.view),
                WriteDescriptor::StructuredBufferSrv(1, draw_info.main_view_indices_view.view),
            };

            const std::array writes_1 = {
                WriteDescriptor::StructuredBufferSrv(0, lights_info.point_lights_view.view),
                WriteDescriptor::StructuredBufferSrv(1, lights_info.directional_lights_view.view),
                WriteDescriptor::StructuredBufferSrv(
                    2, BufferResourceView::create(resources.get_buffer(data.visible_point_light_indices))),
                WriteDescriptor::ConstantBuffer(0, culling_info.light_culling_info.view),
                WriteDescriptor::TextureSrv(
                    3, ImageResourceView::create(resources.get_image(data.directional_shadow_map))),
                WriteDescriptor::SamplerState(0, directional_shadow_map_sampler),
                WriteDescriptor::StructuredBufferSrv(4, shadows_info.cascade_splits_view.view),
                WriteDescriptor::StructuredBufferSrv(5, lights_info.cascade_light_space_matrices_view.view),
                WriteDescriptor::SamplerState(1, get_sampler_state({})),
            };

            const auto descriptor_set_0 = g_render_device->allocate_descriptor_set(
                LightingLayout_0::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_0->update(writes_0);

            const auto descriptor_set_1 = g_render_device->allocate_descriptor_set(
                LightingLayout_1::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_1->update(writes_1);

            DepthStencilState depth_stencil{};
            depth_stencil.depth_test = true;
            depth_stencil.depth_write = false;
            depth_stencil.depth_compare_op = DepthStencilState::DepthCompareOp::LessEqual;

            command.begin_render_pass(pass_info);
            {
                const DrawList& list = m_draw_manager->get_draw_list(draw_info.main_view_handle);

                for (size_t i = 0; i < list.num_blocks; ++i)
                {
                    const DrawBlock& block = list.blocks[i];

                    const std::string draw_block_name = std::format("DrawBlock:{}", block.pipeline_hash);
                    command.begin_gpu_marker(draw_block_name);

                    const auto pipeline = get_graphics_pipeline(
                        block.vertex_instance,
                        block.fragment_instance,
                        RasterizationState{},
                        depth_stencil,
                        ColorBlendState{},
                        framebuffer_info);
                    command.bind_pipeline(pipeline);

                    command.bind_descriptor_set(descriptor_set_0, 0);
                    command.bind_descriptor_set(descriptor_set_1, 1);

                    size_t last_material_hash = 0;
                    for (size_t elem_idx = 0; elem_idx < block.num_elements; ++elem_idx)
                    {
                        const DrawElement& element = block.elements[elem_idx];
                        if (last_material_hash != element.material->get_material_hash())
                        {
                            last_material_hash = element.material->get_material_hash();
                            set_material(command, *element.material);
                        }

                        struct PushConstant
                        {
                            uint64_t transform_offset;
                        } push_constant{};

                        push_constant.transform_offset = element.transform_offset;
                        command.push_constant(push_constant);

                        draw_mesh_instanced(command, *element.mesh, element.instance_count);
                    }

                    command.end_gpu_marker();
                }
            }
            command.end_render_pass();
        });
}

void RenderGraphRenderer::add_light_culling_debug_pass(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard)
    const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    const LightCullingInfo& culling_info = blackboard.get<LightCullingInfo>();

    struct LightCullingDebugData
    {
        RenderGraphResource output_texture;
        RenderGraphResource visible_point_light_indices;
    };

    builder.add_pass<LightCullingDebugData>(
        "LightCullingDebug",
        [&](RenderGraphPassBuilder2& pass, LightCullingDebugData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);

            data.output_texture = pass.attachment(frame_info.output_texture);
            data.visible_point_light_indices = pass.read(culling_info.visible_point_light_indices);
        },
        [=,
         this](CommandBuffer& command, const LightCullingDebugData& data, const RenderGraphPassResources2& resources) {
            FramebufferAttachment2 color_attachment{};
            color_attachment.rtv = ImageResourceView::create(resources.get_image(data.output_texture));
            color_attachment.load_operation = LoadOperation::Load;
            color_attachment.store_operation = StoreOperation::Store;

            RenderPassInfo2 pass_info{};
            pass_info.extent = {frame_info.width, frame_info.height};
            pass_info.color_attachments = {color_attachment};

            FramebufferInfo framebuffer_info{};
            framebuffer_info.color_attachments = {resources.get_image(data.output_texture)->get_format()};

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightCullingDebugLayout_0)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Fragment)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes_0 = {
                WriteDescriptor::StructuredBufferSrv(
                    0, BufferResourceView::create(resources.get_buffer(data.visible_point_light_indices))),
                WriteDescriptor::ConstantBuffer(0, culling_info.light_culling_info.view),
            };

            const auto descriptor_set_0 = g_render_device->allocate_descriptor_set(
                LightCullingDebugLayout_0::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_0->update(writes_0);

            DepthStencilState depth_stencil{};
            depth_stencil.depth_test = false;
            depth_stencil.depth_write = false;

            ColorBlendState color_blend{};
            color_blend.method = ColorBlendState::Method::PerAttachment;
            color_blend.attachments = {
                ColorBlendState::AttachmentState{
                    .blend_enabled = true,
                    .src_color_blend_factor = ColorBlendState::BlendFactor::SourceAlpha,
                    .dst_color_blend_factor = ColorBlendState::BlendFactor::OneMinusSourceAlpha,
                    .color_blend_op = ColorBlendState::BlendOperation::Add,
                    .src_alpha_blend_factor = ColorBlendState::BlendFactor::One,
                    .dst_alpha_blend_factor = ColorBlendState::BlendFactor::Zero,
                    .alpha_blend_op = ColorBlendState::BlendOperation::Add,
                    .color_write_mask = ColorBlendState::ColorComponentBits::All,
                },
            };

            command.begin_render_pass(pass_info);
            {
                const auto pipeline = get_graphics_pipeline(
                    LightCullingDebugShaderVS{},
                    LightCullingDebugShaderFS{},
                    RasterizationState{},
                    depth_stencil,
                    color_blend,
                    framebuffer_info);
                command.bind_pipeline(pipeline);

                command.bind_descriptor_set(descriptor_set_0, 0);

                command.draw(*m_fullscreen_triangle);
            }
            command.end_render_pass();
        });
}

void RenderGraphRenderer::add_cascaded_shadow_mapping_debug_pass(
    RenderGraphBuilder2& builder,
    RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    const DepthNormalsPrepassInfo& depth_normals_info = blackboard.get<DepthNormalsPrepassInfo>();
    const ShadowsInfo& shadows_info = blackboard.get<ShadowsInfo>();

    const auto sampler = get_sampler_state({});

    ColorBlendState color_blend{};
    color_blend.method = ColorBlendState::Method::PerAttachment;
    color_blend.attachments = {
        ColorBlendState::AttachmentState{
            .blend_enabled = true,
            .src_color_blend_factor = ColorBlendState::BlendFactor::SourceAlpha,
            .dst_color_blend_factor = ColorBlendState::BlendFactor::OneMinusSourceAlpha,
            .color_blend_op = ColorBlendState::BlendOperation::Add,
            .src_alpha_blend_factor = ColorBlendState::BlendFactor::One,
            .dst_alpha_blend_factor = ColorBlendState::BlendFactor::Zero,
            .alpha_blend_op = ColorBlendState::BlendOperation::Add,
            .color_write_mask = ColorBlendState::ColorComponentBits::All,
        },
    };

    DepthStencilState depth_stencil{};
    depth_stencil.depth_test = false;
    depth_stencil.depth_write = false;

    struct DrawCascadesData
    {
        RenderGraphResource output_texture;
        RenderGraphResource depth_texture;
    };

    builder.add_pass<DrawCascadesData>(
        "DrawCascades",
        [&](RenderGraphPassBuilder2& pass, DrawCascadesData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);
            data.output_texture = pass.attachment(frame_info.output_texture);
            data.depth_texture = pass.read(depth_normals_info.depth_texture);
        },
        [=, this](CommandBuffer& command, const DrawCascadesData& data, const RenderGraphPassResources2& resources) {
            FramebufferAttachment2 color_attachment{};
            color_attachment.rtv = ImageResourceView::create(resources.get_image(data.output_texture));
            color_attachment.load_operation = LoadOperation::Load;
            color_attachment.store_operation = StoreOperation::Store;

            RenderPassInfo2 pass_info{};
            pass_info.extent = {frame_info.width, frame_info.height};
            pass_info.color_attachments = {color_attachment};

            FramebufferInfo framebuffer_info{};
            framebuffer_info.color_attachments = {resources.get_image(data.output_texture)->get_format()};

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(CascadedShadowMappingDebugCascadesLayout_0)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Fragment)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()

            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(CascadedShadowMappingDebugCascadesLayout_1)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(1, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(0, 1, ShaderType::Fragment)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes_0 = {
                WriteDescriptor::ConstantBuffer(0, frame_info.camera_info_view.view),
            };

            const std::array writes_1 = {
                WriteDescriptor::StructuredBufferSrv(0, shadows_info.cascade_splits_view.view),
                WriteDescriptor::TextureSrv(1, ImageResourceView::create(resources.get_image(data.depth_texture))),
                WriteDescriptor::SamplerState(0, sampler),
            };

            const auto descriptor_set_0 = g_render_device->allocate_descriptor_set(
                CascadedShadowMappingDebugCascadesLayout_0::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_0->update(writes_0);

            const auto descriptor_set_1 = g_render_device->allocate_descriptor_set(
                CascadedShadowMappingDebugCascadesLayout_1::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_1->update(writes_1);

            command.begin_render_pass(pass_info);
            {
                const auto pipeline = get_graphics_pipeline(
                    CascadedShadowMappingDebugShaderVS{},
                    CascadedShadowMappingDebugCascadesShaderFS{},
                    RasterizationState{},
                    depth_stencil,
                    color_blend,
                    framebuffer_info);
                command.bind_pipeline(pipeline);

                command.bind_descriptor_set(descriptor_set_0, 0);
                command.bind_descriptor_set(descriptor_set_1, 1);

                command.draw(*m_fullscreen_triangle);
            }
            command.end_render_pass();
        });

    const float shadow_map_width = glm::round(static_cast<float>(frame_info.width) * 0.5f);
    const float shadow_map_height = glm::round(static_cast<float>(frame_info.height) * 0.3f);

    struct DrawShadowMapData
    {
        RenderGraphResource output_texture;
        RenderGraphResource shadow_map_texture;
    };

    builder.add_pass<DrawShadowMapData>(
        "DrawShadowMap",
        [&](RenderGraphPassBuilder2& pass, DrawShadowMapData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);
            data.output_texture = pass.attachment(frame_info.output_texture);
            data.shadow_map_texture = pass.read(shadows_info.shadow_map_texture);
        },
        [=, this](CommandBuffer& command, const DrawShadowMapData& data, const RenderGraphPassResources2& resources) {
            FramebufferAttachment2 color_attachment{};
            color_attachment.rtv = ImageResourceView::create(resources.get_image(data.output_texture));
            color_attachment.load_operation = LoadOperation::Load;
            color_attachment.store_operation = StoreOperation::Store;

            RenderPassInfo2 pass_info{};
            pass_info.extent = {static_cast<uint32_t>(shadow_map_width), static_cast<uint32_t>(shadow_map_height)};
            pass_info.color_attachments = {color_attachment};

            FramebufferInfo framebuffer_info{};
            framebuffer_info.color_attachments = {resources.get_image(data.output_texture)->get_format()};

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(CascadedShadowMappingDebugTextureLayout_1)
                MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(0, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(2, 1, ShaderType::Fragment)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes_1 = {
                WriteDescriptor::SamplerState(0, sampler),
                WriteDescriptor::TextureSrv(2, ImageResourceView::create(resources.get_image(data.shadow_map_texture))),
            };

            const auto descriptor_set_1 = g_render_device->allocate_descriptor_set(
                CascadedShadowMappingDebugTextureLayout_1::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_1->update(writes_1);

            command.begin_render_pass(pass_info);
            {
                const auto pipeline = get_graphics_pipeline(
                    CascadedShadowMappingDebugShaderVS{},
                    CascadedShadowMappingDebugTextureShaderFS{},
                    RasterizationState{},
                    depth_stencil,
                    ColorBlendState{},
                    framebuffer_info);
                command.bind_pipeline(pipeline);

                command.bind_descriptor_set(descriptor_set_1, 1);

                command.draw(*m_fullscreen_triangle);
            }
            command.end_render_pass();
        });
}

void RenderGraphRenderer::get_light_information(RenderGraphBlackboard& blackboard)
{
    MIZU_PROFILE_SCOPED;

    FrameLinearAllocator& frame_allocator = *blackboard.get<FrameInfo>().frame_allocator;

    const CascadedShadowsSettings& shadow_settings = blackboard.get<RenderGraphRendererSettings>().cascaded_shadows;
    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();

    m_point_lights.clear();
    m_directional_lights.clear();
    m_cascade_light_space_matrices.clear();

    const LightStateManager::IteratorWrapper& wrapper = g_light_state_manager->rend_iterator();

    m_point_lights.reserve(wrapper.size());
    m_directional_lights.reserve(wrapper.size());
    m_cascade_light_space_matrices.reserve(wrapper.size() * shadow_settings.num_cascades);

    for (const LightHandle& handle : wrapper)
    {
        const LightDynamicState& dynamic_state = g_light_state_manager->get_dynamic_state(handle);

        if (handle->is_point_light())
        {
            GpuPointLight light{};
            light.position = handle->get_position();
            light.color = dynamic_state.color;
            light.intensity = dynamic_state.intensity;
            light.cast_shadows = dynamic_state.cast_shadows;
            light.radius = std::get<LightDynamicState::Point>(dynamic_state.data).radius;

            m_point_lights.push_back(light);
        }
        else if (handle->is_directional_light())
        {
            GpuDirectionalLight light{};
            light.position = handle->get_position();
            light.color = dynamic_state.color;
            light.intensity = dynamic_state.intensity;
            light.cast_shadows = dynamic_state.cast_shadows;
            light.direction = std::get<LightDynamicState::Directional>(dynamic_state.data).direction;

            m_directional_lights.push_back(light);
        }
        else
        {
            MIZU_UNREACHABLE("Invalid light type in LightStateManager");
        }
    }

    for (const GpuDirectionalLight& light : m_directional_lights)
    {
        if (!light.cast_shadows)
            continue;

        for (uint32_t cascade_idx = 0; cascade_idx < shadow_settings.num_cascades; ++cascade_idx)
        {
            const float split_dist = shadow_settings.cascade_split_factors[cascade_idx];
            const float last_split_dist =
                cascade_idx == 0 ? 0.0f : shadow_settings.cascade_split_factors[cascade_idx - 1];

            // clang-format off
            glm::vec3 frustum_corners[8] = {
                glm::vec3(-1.0f,  1.0f, 0.0f),
                glm::vec3( 1.0f,  1.0f, 0.0f),
                glm::vec3( 1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f,  1.0f, 1.0f),
                glm::vec3( 1.0f,  1.0f, 1.0f),
                glm::vec3( 1.0f, -1.0f, 1.0f),
                glm::vec3(-1.0f, -1.0f, 1.0f),
            };
            // clang-format on

            for (uint32_t i = 0; i < 8; ++i)
            {
                const glm::vec4 inverted_corner =
                    frame_info.camera_info.inverseViewProj * glm::vec4(frustum_corners[i], 1.0f);
                frustum_corners[i] = inverted_corner / inverted_corner.w;
            }

            for (uint32_t i = 0; i < 4; ++i)
            {
                const glm::vec3 dist = frustum_corners[i + 4] - frustum_corners[i];
                frustum_corners[i + 4] = frustum_corners[i] + (dist * split_dist);
                frustum_corners[i] = frustum_corners[i] + (dist * last_split_dist);
            }

            glm::vec3 frustum_center{0.0f};
            for (uint32_t i = 0; i < 8; ++i)
            {
                frustum_center += frustum_corners[i];
            }
            frustum_center /= 8.0f;

            float radius = 0.0f;
            for (uint32_t i = 0; i < 8; ++i)
            {
                const float distance = glm::length(frustum_corners[i] - frustum_center);
                radius = glm::max(radius, distance);
            }
            radius = glm::ceil(radius * 16.0f) / 16.0f;

            const glm::vec3 max_extents = glm::vec3(radius);
            const glm::vec3 min_extents = -max_extents;

            const glm::vec3 cascade_extents = max_extents - min_extents;
            const glm::vec3 camera_pos = frustum_center - light.direction * -min_extents.z;

            const glm::mat4 light_view = glm::lookAt(camera_pos, frustum_center, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 light_proj =
                glm::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.0f, cascade_extents.z);

            const glm::mat4 light_view_proj = light_proj * light_view;
            m_cascade_light_space_matrices.push_back(light_view_proj);
        }
    }

    const FrameAllocation point_lights = frame_allocator.allocate_structured<GpuPointLight>(m_point_lights.size());
    point_lights.upload(m_point_lights);

    const FrameAllocation directional_lights =
        frame_allocator.allocate_structured<GpuDirectionalLight>(m_directional_lights.size());
    directional_lights.upload(m_directional_lights);

    const FrameAllocation cascade_light_space_matrices =
        frame_allocator.allocate_structured<glm::mat4>(m_cascade_light_space_matrices.size());
    cascade_light_space_matrices.upload(m_cascade_light_space_matrices);

    LightsInfo& lights_info = blackboard.add<LightsInfo>();
    lights_info.point_lights_view = point_lights;
    lights_info.directional_lights_view = directional_lights;
    lights_info.cascade_light_space_matrices_view = cascade_light_space_matrices;
}

void RenderGraphRenderer::create_draw_lists(RenderGraphBlackboard& blackboard)
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    FrameLinearAllocator& frame_allocator = *blackboard.get<FrameInfo>().frame_allocator;

    const Job transform_info_job = Job::create([this]() {
        MIZU_PROFILE_SCOPED_NAME("generate_transform_info_job");

        const StaticMeshStateManager::IteratorWrapper& wrapper = g_static_mesh_state_manager->rend_iterator();
        for (uint32_t i = 0; i < wrapper.size(); ++i)
        {
            const StaticMeshHandle& handle = *(wrapper.begin() + i);
            const StaticMeshStaticState& ss = g_static_mesh_state_manager->rend_get_static_state(handle);

            const glm::vec3 translation = ss.transform_handle->get_translation();
            const glm::vec3 rotation = ss.transform_handle->get_rotation();
            const glm::vec3 scale = ss.transform_handle->get_scale();

            glm::mat4 transform{1.0f};
            transform = glm::translate(transform, translation);
            transform = glm::rotate(transform, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            transform = glm::rotate(transform, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            transform = glm::rotate(transform, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
            transform = glm::scale(transform, scale);

            m_transform_info_buffer[i] = InstanceTransformInfo{
                .transform = transform,
                .normal_matrix = glm::transpose(glm::inverse(transform)),
            };
        }
    });

    DrawListHandle main_view_handle, shadows_view_handle;

    const Job main_view_job = Job::create(
        [this, frame_info](DrawListHandle& output) {
            const Frustum frustum =
                Frustum::from_view_projection(frame_info.camera_info.viewProj, frame_info.camera_info.pos);
            output =
                m_draw_manager->create_draw_list(DrawListType::Opaque, frustum, m_main_view_transform_indices_buffer);
        },
        std::ref(main_view_handle));

    const Job cascaded_shadows_job = Job::create(
        [this](DrawListHandle& output) {
            // TODO: Using no frustum to include all elements into the cascaded shadows
            output = m_draw_manager->create_draw_list(
                DrawListType::Shadow, Frustum{}, m_shadows_view_transform_indices_buffer);
        },
        std::ref(shadows_view_handle));

    Job jobs[] = {transform_info_job, main_view_job, cascaded_shadows_job};
    const JobSystemHandle handle = g_job_system->schedule(jobs);

    handle.wait();

    const FrameAllocation transform_indices =
        frame_allocator.allocate_structured<InstanceTransformInfo>(m_transform_info_buffer.size());
    transform_indices.upload(m_transform_info_buffer);

    const FrameAllocation main_view_indices =
        frame_allocator.allocate_structured<uint64_t>(m_main_view_transform_indices_buffer.size());
    main_view_indices.upload(m_main_view_transform_indices_buffer);

    const FrameAllocation shadows_view_indices =
        frame_allocator.allocate_structured<uint64_t>(m_shadows_view_transform_indices_buffer.size());
    shadows_view_indices.upload(m_shadows_view_transform_indices_buffer);

    DrawInfo& draw_info = blackboard.add<DrawInfo>();
    draw_info.main_view_handle = main_view_handle;
    draw_info.shadows_view_handle = shadows_view_handle;
    draw_info.transform_info_view = transform_indices;
    draw_info.main_view_indices_view = main_view_indices;
    draw_info.shadows_indices_view = shadows_view_indices;
}

} // namespace Mizu
