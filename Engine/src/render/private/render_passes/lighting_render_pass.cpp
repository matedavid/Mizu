#include "render_passes/lighting_render_pass.h"

#include "render_core/rhi/rhi_helpers.h"

#include "render.pipeline/scene_renderer_shaders.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/scene/scene_blackboard_data.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "render_passes/depth_render_pass.h"
#include "render_passes/gbuffer_render_pass.h"
#include "render_passes/shadow_render_pass.h"

namespace Mizu
{

LightCullingData create_light_culling_data(
    RenderGraphBuilder& builder,
    uint32_t width,
    uint32_t height,
    FrameLinearAllocator& frame_allocator)
{
    const glm::uvec3 group_count = compute_group_count(
        {width, height, 1.0f}, {LightCullingShaderCS::TILE_SIZE, LightCullingShaderCS::TILE_SIZE, 1.0f});

    const uint64_t tile_visible_lights_count =
        group_count.x * group_count.y * LightCullingShaderCS::MAX_LIGHTS_PER_TILE;

    const LightCullingData::GpuLightCullingInfo light_culling_info{
        .num_tiles = glm::uvec2(group_count),
    };

    const FrameAllocation light_culling_info_allocation =
        frame_allocator.allocate_constant<LightCullingData::GpuLightCullingInfo>();
    light_culling_info_allocation.upload(light_culling_info);

    LightCullingData data{};
    data.light_culling_info = light_culling_info;
    data.tile_visible_lights =
        builder.create_structured_buffer<uint16_t>(tile_visible_lights_count, "TileVisibleLights");
    data.light_culling_info_allocation = light_culling_info_allocation;

    return data;
}

void add_light_culling_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    const RenderViewData& view_data = blackboard.get<RenderViewData>();
    const DepthData& depth_data = blackboard.get<DepthData>();
    const LightsData& lights_data = blackboard.get<LightsData>();
    const LightCullingData& light_culling_data = blackboard.get<LightCullingData>();

    struct PassData
    {
        RenderGraphResource tile_visible_lights;
        RenderGraphResource depth;
    };

    builder.add_pass<PassData>(
        "LightCullingPass",
        [&](RenderGraphPassBuilder& pass, PassData& data) {
            pass.set_hint(RenderGraphPassHint::Compute);

            data.tile_visible_lights = pass.write(light_culling_data.tile_visible_lights);
            data.depth = pass.read(depth_data.depth);
        },
        [=](CommandBuffer& command, const PassData& data, const RenderGraphPassResources& resources) {
            const auto tile_visible_lights_buffer = resources.get_buffer(data.tile_visible_lights);
            const auto depth_texture = resources.get_image(data.depth);

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightCulling_Layout)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Compute)       // g_camera_info
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Compute) // g_point_lights
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(1, 1, ShaderType::Compute)       // g_light_culling_info
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_UAV(0, 1, ShaderType::Compute) // g_visible_point_light_indices
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(1, 1, ShaderType::Compute)           // g_depth
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::ConstantBuffer(0, view_data.camera_allocation.view),
                WriteDescriptor::StructuredBufferSrv(0, lights_data.point_lights_allocation.view),
                WriteDescriptor::ConstantBuffer(1, light_culling_data.light_culling_info_allocation.view),
                WriteDescriptor::StructuredBufferUav(0, BufferResourceView::create(tile_visible_lights_buffer)),
                WriteDescriptor::TextureSrv(1, ImageResourceView::create(depth_texture)),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                LightCulling_Layout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            const auto pipeline = get_compute_pipeline(LightCullingShaderCS{});
            command.bind_pipeline(pipeline);

            command.bind_descriptor_set(descriptor_set, 0);

            const glm::uvec2 num_tiles = light_culling_data.light_culling_info.num_tiles;
            command.dispatch({num_tiles.x, num_tiles.y, 1});
        });
}

void add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    const RenderViewData& view_data = blackboard.get<RenderViewData>();
    const DepthData& depth_data = blackboard.get<DepthData>();
    const GbufferData& gbuffer_data = blackboard.get<GbufferData>();
    const LightsData& lights_data = blackboard.get<LightsData>();
    const LightCullingData& light_culling_data = blackboard.get<LightCullingData>();
    const LightingData& lighting_data = blackboard.get<LightingData>();
    const CascadedShadowData& cascaded_shadow_data = blackboard.get<CascadedShadowData>();

    const auto directional_shadow_map_sampler = get_sampler_state(
        SamplerStateDescription{
            .address_mode_u = ImageAddressMode::ClampToEdge,
            .address_mode_v = ImageAddressMode::ClampToEdge,
            .address_mode_w = ImageAddressMode::ClampToEdge,
            .border_color = BorderColor::FloatOpaqueWhite,
        });

    struct PassData
    {
        RenderGraphResource gbuffer0;
        RenderGraphResource gbuffer1;
        RenderGraphResource gbuffer2;
        RenderGraphResource depth;

        RenderGraphResource tile_visible_lights;
        RenderGraphResource directional_shadow_map;

        RenderGraphResource output;
    };

    builder.add_pass<PassData>(
        "LightingPass",
        [&](RenderGraphPassBuilder& pass, PassData& data) {
            pass.set_hint(RenderGraphPassHint::Compute);

            data.gbuffer0 = pass.read(gbuffer_data.gbuffer0);
            data.gbuffer1 = pass.read(gbuffer_data.gbuffer1);
            data.gbuffer2 = pass.read(gbuffer_data.gbuffer2);
            data.depth = pass.read(depth_data.depth);

            data.tile_visible_lights = pass.read(light_culling_data.tile_visible_lights);
            data.directional_shadow_map = pass.read(cascaded_shadow_data.cascaded_shadow_atlas);

            data.output = pass.write(lighting_data.lighting_output);
        },
        [=](CommandBuffer& command, const PassData& data, const RenderGraphPassResources& resources) {
            const auto gbuffer0 = resources.get_image(data.gbuffer0);
            const auto gbuffer1 = resources.get_image(data.gbuffer1);
            const auto gbuffer2 = resources.get_image(data.gbuffer2);
            const auto depth = resources.get_image(data.depth);
            const auto tile_visible_lights = resources.get_buffer(data.tile_visible_lights);
            const auto directional_shadow_map = resources.get_image(data.directional_shadow_map);
            const auto output = resources.get_image(data.output);

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightingPass_Layout0)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Compute) // g_camera_info
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(0, 1, ShaderType::Compute)     // g_gbuffer0
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(1, 1, ShaderType::Compute)     // g_gbuffer1
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(2, 1, ShaderType::Compute)     // g_gbuffer2
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(3, 1, ShaderType::Compute)     // g_depth
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_UAV(0, 1, ShaderType::Compute)     // g_output
            MIZU_END_DESCRIPTOR_SET_LAYOUT()

            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightingPass_Layout1)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Compute) // g_point_lights
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Compute) // g_directional_lights
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(2, 1, ShaderType::Compute) // g_tile_visible_lights
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Compute)       // g_light_culling_info
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(3, 1, ShaderType::Compute)           // g_directional_shadow_map
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(4, 1, ShaderType::Compute) // g_cascade_splits
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(5, 1, ShaderType::Compute) // g_light_space_matrices
                MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(0, 1, ShaderType::Compute)         // g_shadow_map_sampler
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes_0 = {
                WriteDescriptor::ConstantBuffer(0, view_data.camera_allocation.view),
                WriteDescriptor::TextureSrv(0, ImageResourceView::create(gbuffer0)),
                WriteDescriptor::TextureSrv(1, ImageResourceView::create(gbuffer1)),
                WriteDescriptor::TextureSrv(2, ImageResourceView::create(gbuffer2)),
                WriteDescriptor::TextureSrv(3, ImageResourceView::create(depth)),
                WriteDescriptor::TextureUav(0, ImageResourceView::create(output)),
            };

            const std::array writes_1 = {
                WriteDescriptor::StructuredBufferSrv(0, lights_data.point_lights_allocation.view),
                WriteDescriptor::StructuredBufferSrv(1, lights_data.directional_lights_allocation.view),
                WriteDescriptor::StructuredBufferSrv(2, BufferResourceView::create(tile_visible_lights)),
                WriteDescriptor::ConstantBuffer(0, light_culling_data.light_culling_info_allocation.view),
                WriteDescriptor::TextureSrv(3, ImageResourceView::create(directional_shadow_map)),
                WriteDescriptor::StructuredBufferSrv(4, cascaded_shadow_data.cascade_splits_allocation.view),
                WriteDescriptor::StructuredBufferSrv(
                    5, cascaded_shadow_data.cascade_light_space_matrices_allocation.view),
                WriteDescriptor::SamplerState(0, directional_shadow_map_sampler),
            };

            const auto descriptor_set_0 = g_render_device->allocate_descriptor_set(
                LightingPass_Layout0::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_0->update(writes_0);

            const auto descriptor_set_1 = g_render_device->allocate_descriptor_set(
                LightingPass_Layout1::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set_1->update(writes_1);

            const auto pipeline = get_compute_pipeline(LightingShaderCS{});
            command.bind_pipeline(pipeline);

            command.bind_descriptor_set(descriptor_set_0, 0);
            command.bind_descriptor_set(descriptor_set_1, 1);

            const glm::uvec3 group_count = compute_group_count(
                {view_data.width, view_data.height, 1},
                {LightingShaderCS::GROUP_COUNT, LightingShaderCS::GROUP_COUNT, 1});

            command.dispatch(group_count);
        });
}

} // namespace Mizu