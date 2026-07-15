#include "render_passes/gbuffer_render_pass.h"

#include "render_core/rhi/render_pass.h"

#include "registries/render_view_registry.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/scene/draw_list_system.h"
#include "render/scene/scene_blackboard_data.h"
#include "render/systems/sampler_state_cache.h"
#include "render_passes/depth_render_pass.h"
#include "resources/residency_system.h"

namespace Mizu
{

class GBufferRasterPass : public MaterialShaderRasterPass
{
};

MIZU_IMPLEMENT_DRAW_LIST_RASTER_PASS(GBufferRasterPass);

GbufferData create_gbuffer_data(RenderGraphBuilder& builder, uint32_t width, uint32_t height)
{
    GbufferData data{};
    data.gbuffer0 = builder.create_texture2d(width, height, ImageFormat::R16G16_SFLOAT, "Gbuffer0_Normals");
    data.gbuffer1 = builder.create_texture2d(width, height, ImageFormat::R32G32B32A32_SFLOAT, "Gbuffer1_Albedo");
    data.gbuffer2 = builder.create_texture2d(width, height, ImageFormat::R16G16B16A16_SFLOAT, "Gbuffer2_Material");

    return data;
}

void add_gbuffer_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    const RenderViewData& view_data = blackboard.get<RenderViewData>();
    const RenderSystemsData& systems_data = blackboard.get<RenderSystemsData>();
    const GbufferData& gbuffer_data = blackboard.get<GbufferData>();
    const DepthData& depth_data = blackboard.get<DepthData>();

    struct PassData
    {
        RenderGraphResource gbuffer0;
        RenderGraphResource gbuffer1;
        RenderGraphResource gbuffer2;
        RenderGraphResource depth;

        DrawListHandle draw_list_handle;
    };

    builder.add_pass<PassData>(
        "GbufferPass",
        [&](RenderGraphPassBuilder& pass, PassData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);

            data.gbuffer0 = pass.attachment(gbuffer_data.gbuffer0);
            data.gbuffer1 = pass.attachment(gbuffer_data.gbuffer1);
            data.gbuffer2 = pass.attachment(gbuffer_data.gbuffer2);
            data.depth = pass.attachment(depth_data.depth);

            data.draw_list_handle = create_draw_list({
                .raster_pass = get_GBufferRasterPass(),
                .frustum = view_data.data.frustum,
            });
        },
        [=](CommandBuffer& command, const PassData& data, const RenderGraphPassResources& resources) {
            const FramebufferAttachment gbuffer0_attachment =
                resources.get_framebuffer_attachment(data.gbuffer0, LoadOperation::Clear, StoreOperation::Store);
            const FramebufferAttachment gbuffer1_attachment =
                resources.get_framebuffer_attachment(data.gbuffer1, LoadOperation::Clear, StoreOperation::Store);
            const FramebufferAttachment gbuffer2_attachment =
                resources.get_framebuffer_attachment(data.gbuffer2, LoadOperation::Clear, StoreOperation::Store);
            const FramebufferAttachment depth_attachment = resources.get_framebuffer_attachment(
                data.depth, LoadOperation::Clear, StoreOperation::Store, glm::vec4(1.0f));

            RenderPassInfo render_pass{};
            render_pass.extent = {view_data.width, view_data.height};
            render_pass.color_attachments = {
                gbuffer0_attachment,
                gbuffer1_attachment,
                gbuffer2_attachment,
            };
            render_pass.depth_stencil_attachment = depth_attachment;

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(GbufferPass_Layout)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Vertex)         // g_cameraInfo
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Fragment) // g_materialBuffer
                MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(0, 1, ShaderType::Fragment)         // g_sampler
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::ConstantBuffer(0, view_data.camera_allocation.view),
                WriteDescriptor::StructuredBufferSrv(
                    0, BufferResourceView::create(systems_data.material_residency_system.get_material_buffer())),
                WriteDescriptor::SamplerState(0, get_sampler_state({})),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                GbufferPass_Layout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            DrawListRasterBindings bindings{};
            bindings.add(1, descriptor_set);
            bindings.add(2, systems_data.texture_residency_system.get_bindless_descriptor_set());

            command.begin_render_pass(render_pass);
            {
                const DrawListRasterPassInfo raster_pass_info{
                    .depth_stencil_state =
                        DepthStencilState{
                            .depth_test = true,
                            .depth_write = true,
                        },
                    .framebuffer_info = create_framebuffer_info(render_pass),
                    .bindings = bindings,
                };

                dispatch_draw_list(command, data.draw_list_handle, raster_pass_info);
            }
            command.end_render_pass();
        });
}

} // namespace Mizu