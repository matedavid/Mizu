#include "render_passes/lighting_render_pass.h"

#include "render_core/rhi/rhi_helpers.h"

#include "render.pipeline/scene_renderer_shaders.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/scene/scene_blackboard_data.h"
#include "render/systems/pipeline_cache.h"
#include "render_passes/depth_render_pass.h"
#include "render_passes/gbuffer_render_pass.h"

namespace Mizu
{

void add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    const RenderViewData& view_data = blackboard.get<RenderViewData>();
    const DepthData& depth_data = blackboard.get<DepthData>();
    const GbufferData& gbuffer_data = blackboard.get<GbufferData>();
    const LightsData& lights_data = blackboard.get<LightsData>();
    const LightingData& lighting_data = blackboard.get<LightingData>();

    struct PassData
    {
        RenderGraphResource gbuffer0;
        RenderGraphResource gbuffer1;
        RenderGraphResource gbuffer2;
        RenderGraphResource depth;
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
            data.output = pass.write(lighting_data.lighting_output);
        },
        [=](CommandBuffer& command, const PassData& data, const RenderGraphPassResources& resources) {
            const auto gbuffer0 = resources.get_image(data.gbuffer0);
            const auto gbuffer1 = resources.get_image(data.gbuffer1);
            const auto gbuffer2 = resources.get_image(data.gbuffer2);
            const auto depth = resources.get_image(data.depth);
            const auto output = resources.get_image(data.output);

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightingPass_Layout0)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Compute) // g_cameraInfo
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(0, 1, ShaderType::Compute)     // g_gbuffer0
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(1, 1, ShaderType::Compute)     // g_gbuffer1
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(2, 1, ShaderType::Compute)     // g_gbuffer2
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(3, 1, ShaderType::Compute)     // g_depth
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_UAV(0, 1, ShaderType::Compute)     // g_output
            MIZU_END_DESCRIPTOR_SET_LAYOUT()

            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightingPass_Layout1)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Compute) // g_pointLights
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Compute) // g_directionalLights
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