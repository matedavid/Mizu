#include "render/scene/scene_renderer.h"

#include <vector>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "registries/render_view_registry.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/scene/scene_blackboard_data.h"
#include "render/scene/scene_renderer_extensions.h"
#include "render_passes/depth_render_pass.h"
#include "render_passes/gbuffer_render_pass.h"

namespace Mizu
{

SceneRenderer::SceneRenderer() {}

static uint32_t viewport_float_to_uint(float relative, uint32_t absolute)
{
    return static_cast<uint32_t>(std::round(relative * static_cast<float>(absolute)));
}

void SceneRenderer::build_render_graph(
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard,
    const RenderModuleFrameData& frame_data)
{
    const std::span<const RenderViewRegistryEntry> views = render_view_registry_get_views();

    if (views.empty())
    {
        MIZU_LOG_ERROR("No RenderView has been created, nothing to render");
        return;
    }

    const ImageDescription& frame_output_desc = builder.get_image_desc(frame_data.output_texture);

    // If we only have one RenderView, and this view matches the frame output dimensions with offset 0, use the
    // swapchain image directly without composition.
    const bool use_output_texture = [&]() {
        if (views.size() != 1)
            return false;

        const RenderViewRegistryEntry& single = views[0];

        const ViewportRect& viewport = single.viewport;
        // clang-format off
        return viewport.extent.x == 1.0f
            && viewport.extent.y == 1.0f
            && viewport.offset.x == 0.0f
            && viewport.offset.y == 0.0f;
        // clang-format on
    }();

    std::vector<RenderGraphResource> view_outputs{};

    for (const RenderViewRegistryEntry& view : views)
    {
        const ViewportRect& viewport = view.viewport;

        RenderViewData& view_data = blackboard.add<RenderViewData>({
            .data = view,
            .width = viewport_float_to_uint(viewport.extent.x, frame_output_desc.width),
            .height = viewport_float_to_uint(viewport.extent.y, frame_output_desc.height),
            .offsetx = viewport_float_to_uint(viewport.offset.x, frame_output_desc.width),
            .offsety = viewport_float_to_uint(viewport.offset.y, frame_output_desc.height),
            .layer = view.layer,
            .view_output_texture = RenderGraphResource{},
        });

        if (use_output_texture)
        {
            view_data.view_output_texture = frame_data.output_texture;
        }
        else
        {
            ImageDescription desc{};
            desc.width = view_data.width;
            desc.height = view_data.height;
            desc.format = ImageFormat::R8G8B8A8_UNORM;
            desc.flags = ImageFlagBits::MutableFormat;
            desc.name = std::format("ViewOutput_{}", view_data.layer);

            view_data.view_output_texture = builder.create_texture(desc);
            view_outputs.push_back(view_data.view_output_texture);
        }

        draw_view(builder, blackboard);

        blackboard.remove<RenderViewData>();
    }

    if (!use_output_texture)
    {
        add_views_composition_pass(builder, blackboard, frame_data, view_outputs);
    }
}

void SceneRenderer::draw_view(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    const RenderViewData& view_data = blackboard.get<RenderViewData>();

    blackboard.add<DepthData>({
        .depth = builder.create_texture2d(view_data.width, view_data.height, ImageFormat::D32_SFLOAT, "Depth"),
    });

    // blackboard.add<GbufferData>(create_gbuffer_data(builder, view_data.width, view_data.height));

    SceneRendererExtensions::execute_extensions(SceneRendererExtensionPoint::FrameBegin, builder, blackboard);

    // add_gbuffer_pass(builder, blackboard);

    SceneRendererExtensions::execute_extensions(SceneRendererExtensionPoint::PostGbuffer, builder, blackboard);

    SceneRendererExtensions::execute_extensions(SceneRendererExtensionPoint::FrameEnd, builder, blackboard);
}

void SceneRenderer::add_views_composition_pass(
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard,
    const RenderModuleFrameData& frame_data,
    std::span<const RenderGraphResource> view_outputs)
{
    (void)builder;
    (void)blackboard;
    (void)frame_data;
    (void)view_outputs;
}

} // namespace Mizu