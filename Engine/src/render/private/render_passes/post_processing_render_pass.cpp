#include "render_passes/post_processing_render_pass.h"

#include "base/debug/assert.h"

#include "render.pipeline/scene_renderer_shaders.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/scene/scene_blackboard_data.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "render/utils/fullscreen_helpers.h"
#include "render_passes/lighting_render_pass.h"

namespace Mizu
{

void add_tonemapping_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    const RenderViewData& view_data = blackboard.get<RenderViewData>();
    const LightingData& lighting_data = blackboard.get<LightingData>();

    struct PassData
    {
        RenderGraphResource input;
        RenderGraphResource output;
    };

    builder.add_pass<PassData>(
        "Tonemapping",
        [&](RenderGraphPassBuilder& pass, PassData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);

            data.input = pass.read(lighting_data.lighting_output);
            data.output = pass.attachment(view_data.view_output_texture);
        },
        [=](CommandBuffer& command, const PassData& data, const RenderGraphPassResources& resources) {
            const auto input_image = resources.get_image(data.input);
            const auto output_image = resources.get_image(data.output);

            FramebufferAttachment color_attachment =
                resources.get_framebuffer_attachment(data.output, LoadOperation::Clear, StoreOperation::Store);

            ImageResourceViewDescription output_view_desc{};
            output_view_desc.override_format = ImageFormat::R8G8B8A8_SRGB;
            color_attachment.rtv = ImageResourceView::create(output_image, output_view_desc);

            RenderPassInfo pass_info{};
            pass_info.extent = {view_data.width, view_data.height};
            pass_info.offset = {static_cast<int32_t>(view_data.offsetx), static_cast<int32_t>(view_data.offsety)};
            pass_info.color_attachments = {color_attachment};

            const FramebufferInfo framebuffer_info = create_framebuffer_info(pass_info);

            DepthStencilState depth_stencil{};
            depth_stencil.depth_write = false;
            depth_stencil.depth_test = false;

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(TonemappingLayout)
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(0, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(0, 1, ShaderType::Fragment)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::TextureSrv(0, ImageResourceView::create(input_image)),
                WriteDescriptor::SamplerState(0, get_sampler_state({})),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                TonemappingLayout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            command.begin_render_pass(pass_info);
            {
                const auto pipeline = get_graphics_pipeline(
                    TonemappingVS{},
                    TonemappingFS{},
                    RasterizationState{},
                    depth_stencil,
                    ColorBlendState{},
                    framebuffer_info);
                command.bind_pipeline(pipeline);

                command.bind_descriptor_set(descriptor_set, 0);

                FullscreenHelpers::draw_fullscreen_triangle(command);
            }
            command.end_render_pass();
        });
}

} // namespace Mizu
