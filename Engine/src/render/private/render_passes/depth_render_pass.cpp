#include "render_passes/depth_render_pass.h"

#include "registries/render_view_registry.h"
#include "render.pipeline/scene_renderer_shaders.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/scene/draw_list_system.h"
#include "render/scene/scene_blackboard_data.h"

namespace Mizu
{

class DepthPrepassRasterPass : public FixedShaderRasterPass
{
  public:
    DepthPrepassRasterPass() : FixedShaderRasterPass(DepthPrepassShaderVS{}, DepthPrepassShaderVS{}) {}
};

MIZU_IMPLEMENT_DRAW_LIST_RASTER_PASS(DepthPrepassRasterPass);

void add_depth_prepass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    const RenderViewData& view_data = blackboard.get<RenderViewData>();
    const DepthData& depth_data = blackboard.get<DepthData>();

    struct PassData
    {
        RenderGraphResource depth;
        DrawListHandle draw_list_handle;
    };

    builder.add_pass<PassData>(
        "DepthPrepass",
        [&](RenderGraphPassBuilder& pass, PassData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);

            data.depth = pass.attachment(depth_data.depth);
            data.draw_list_handle = create_draw_list({
                .raster_pass = get_DepthPrepassRasterPass(),
                .frustum = view_data.data.frustum,
            });
        },
        [=](CommandBuffer& command, const PassData& data, const RenderGraphPassResources& resources) {
            RenderPassInfo render_pass{};
            render_pass.depth_stencil_attachment = resources.get_framebuffer_attachment(
                data.depth, LoadOperation::Clear, StoreOperation::Store, glm::vec4(1.0f));

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(DepthPrePass_Layout)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Vertex) // g_cameraInfo
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::ConstantBuffer(0, view_data.camera_allocation.view),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                DepthPrePass_Layout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            command.begin_render_pass(render_pass);
            {
                const DrawListRasterPassInfo raster_pass_info{
                    .depth_stencil_state =
                        DepthStencilState{
                            .depth_test = true,
                            .depth_write = true,
                        },
                    .framebuffer_info = create_framebuffer_info(render_pass),
                    .bindings = DrawListRasterBindings{}.add(1, descriptor_set),
                };

                dispatch_draw_list(command, data.draw_list_handle, raster_pass_info);
            }
            command.end_render_pass();
        });
}

} // namespace Mizu