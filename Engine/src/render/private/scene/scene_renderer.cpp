#include "render/scene/scene_renderer.h"

#include <vector>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "core/runtime.h"

#include "registries/light_registry.h"
#include "registries/render_settings_registry.h"
#include "registries/render_view_registry.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/scene/scene_blackboard_data.h"
#include "render/scene/scene_renderer_extensions.h"
#include "render_passes/depth_render_pass.h"
#include "render_passes/gbuffer_render_pass.h"
#include "render_passes/lighting_render_pass.h"
#include "render_passes/post_processing_render_pass.h"
#include "render_passes/shadow_render_pass.h"

namespace Mizu
{

static uint32_t viewport_float_to_uint(float relative, uint32_t absolute)
{
    return static_cast<uint32_t>(std::round(relative * static_cast<float>(absolute)));
}

bool SceneRenderer::init()
{
    m_module_container.add_module<CascadedShadowModule>();

    return true;
}

JobHandle SceneRenderer::create_update_jobs(const RenderModuleUpdateContext& ctx)
{
    // We need a coordinator job because we can't guarantee the dependent job that updates the RenderViewRegistry has
    // been executed, which we need to get the information to prepare the update_view jobs.

    return g_job_system
        ->schedule([this]() {
            PendingBatch batch = g_job_system->schedule_batch();

            const std::span<const RenderViewRegistryEntry> views = render_view_registry_get_views();
            m_num_render_views = static_cast<uint32_t>(views.size());

            for (uint32_t i = 0; i < views.size(); ++i)
            {
                batch.add(
                    JobDescription::create(&SceneRenderer::update_view_job, this, i)
                        .name("SceneRenderer::update_view_job"));
            }

            g_job_system->wait_for(batch.submit());
        })
        .depends_on(ctx.wait_job)
        .submit();
}

void SceneRenderer::build_render_graph(
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard,
    const RenderModuleFrameData& frame_data)
{
    MIZU_PROFILE_SCOPED;

    const std::span<const RenderViewRegistryEntry> views = render_view_registry_get_views();

    if (views.empty())
    {
        MIZU_LOG_ERROR("No RenderView has been created, nothing to render");
        return;
    }

    create_lights_data(blackboard);

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

    FrameLinearAllocator& frame_allocator = blackboard.get<RenderSystemsData>().frame_allocator;

    std::vector<RenderGraphResource> view_outputs{};

    for (uint32_t view_id = 0; view_id < m_num_render_views; ++view_id)
    {
        const RenderViewInfo& view_info = m_render_views[view_id];
        const RenderViewRegistryEntry& entry = views[view_id];

        const ViewportRect& viewport = entry.viewport;

        const FrameAllocation camera_allocation = frame_allocator.allocate_constant<GpuCameraInfo>();
        camera_allocation.upload(
            GpuCameraInfo{
                .view = entry.view_matrix,
                .proj = entry.proj_matrix,
                .viewProj = entry.view_proj_matrix,
                .inverseView = glm::inverse(entry.view_matrix),
                .inverseProj = glm::inverse(entry.proj_matrix),
                .inverseViewProj = glm::inverse(entry.view_proj_matrix),
                .pos = entry.camera.position,
                .znear = entry.camera.znear,
                .zfar = entry.camera.zfar,
            });

        RenderGraphBlackboard view_blackboard{blackboard};

        RenderViewData& view_data = view_blackboard.add<RenderViewData>({
            .data = entry,
            .render_settings = view_info.render_settings,
            .width = viewport_float_to_uint(viewport.extent.x, frame_output_desc.width),
            .height = viewport_float_to_uint(viewport.extent.y, frame_output_desc.height),
            .offsetx = viewport_float_to_uint(viewport.offset.x, frame_output_desc.width),
            .offsety = viewport_float_to_uint(viewport.offset.y, frame_output_desc.height),
            .layer = entry.layer,
            .view_id = view_id,
            .camera_allocation = camera_allocation,
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

        draw_view(builder, view_blackboard);
    }

    if (!use_output_texture)
    {
        add_views_composition_pass(builder, blackboard, frame_data, view_outputs);
    }
}

void SceneRenderer::draw_view(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    MIZU_PROFILE_SCOPED;

    const RenderViewData& view_data = blackboard.get<RenderViewData>();
    create_view_blackboards(builder, blackboard, view_data);

    SceneRendererExtensions::execute_extensions(SceneRendererExtensionPoint::FrameBegin, builder, blackboard);

    // TODO: Has to be a setting, and need to take it into account in `add_gbuffer_pass` to not write depth
    constexpr bool depth_prepass_enabled = false;
    if (depth_prepass_enabled)
    {
        add_depth_prepass(builder, blackboard);

        // If depth prepass is enabled, we can start light culling before and potentially overlap with gbuffer if async
        // compute is enabled
        add_light_culling_pass(builder, blackboard);

        SceneRendererExtensions::execute_extensions(SceneRendererExtensionPoint::PostDepth, builder, blackboard);
    }

    add_gbuffer_pass(builder, blackboard);

    if (!depth_prepass_enabled)
    {
        add_light_culling_pass(builder, blackboard);

        SceneRendererExtensions::execute_extensions(SceneRendererExtensionPoint::PostDepth, builder, blackboard);
    }

    SceneRendererExtensions::execute_extensions(SceneRendererExtensionPoint::PostGbuffer, builder, blackboard);

    add_cascaded_shadow_pass(builder, blackboard);

    SceneRendererExtensions::execute_extensions(SceneRendererExtensionPoint::PreLighting, builder, blackboard);

    add_lighting_pass(builder, blackboard);

    SceneRendererExtensions::execute_extensions(SceneRendererExtensionPoint::PostLighting, builder, blackboard);

    add_tonemapping_pass(builder, blackboard);

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

void SceneRenderer::update_view_job(uint32_t view_id)
{
    MIZU_PROFILE_SCOPED;

    const RenderViewRegistryEntry& entry = render_view_registry_get_views()[view_id];

    RenderViewInfo& view_info = m_render_views[view_id];
    view_info.render_settings = render_settings_registry_get().resolve_view_settings(entry);

    m_module_container.get_render_module<CascadedShadowModule>().update_view(view_id, entry, view_info.render_settings);
}

void SceneRenderer::create_view_blackboards(
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard,
    const RenderViewData& view_data)
{
    const RenderSystemsData& systems_data = blackboard.get<RenderSystemsData>();

    blackboard.add<DepthData>({
        .depth = builder.create_texture2d(view_data.width, view_data.height, ImageFormat::D32_SFLOAT, "Depth"),
    });

    blackboard.add<GbufferData>(create_gbuffer_data(builder, view_data.width, view_data.height));

    const CascadedShadowModule& cascaded_shadows_module = m_module_container.get_render_module<CascadedShadowModule>();
    const ShadowRenderSettings& shadow_settings = view_data.render_settings.resolve<ShadowRenderSettings>();
    blackboard.add<CascadedShadowData>(create_cascaded_shadow_data(
        builder,
        systems_data.frame_allocator,
        cascaded_shadows_module.get_view_data(view_data.view_id),
        shadow_settings));

    blackboard.add<LightCullingData>(
        create_light_culling_data(builder, view_data.width, view_data.height, systems_data.frame_allocator));

    blackboard.add<LightingData>({
        .lighting_output = builder.create_texture2d(
            view_data.width, view_data.height, ImageFormat::R32G32B32A32_SFLOAT, "LightingOutput"),
    });
}

void SceneRenderer::create_lights_data(RenderGraphBlackboard& blackboard)
{
    FrameLinearAllocator& frame_allocator = blackboard.get<RenderSystemsData>().frame_allocator;

    const LightRegistry& light_registry = light_registry_get();

    const std::span<const GpuPointLight> point_lights = light_registry.get_point_lights();
    const std::span<const GpuDirectionalLight> directional_lights = light_registry.get_directional_lights();

    LightsData& lights_data = blackboard.add<LightsData>();
    lights_data.point_lights_allocation = frame_allocator.allocate_structured<GpuPointLight>(point_lights.size());
    lights_data.directional_lights_allocation =
        frame_allocator.allocate_structured<GpuDirectionalLight>(directional_lights.size());

    lights_data.num_point_lights = static_cast<uint32_t>(point_lights.size());
    lights_data.num_directional_lights = static_cast<uint32_t>(directional_lights.size());
    lights_data.point_lights_allocation.upload(point_lights);
    lights_data.directional_lights_allocation.upload(directional_lights);
}

} // namespace Mizu