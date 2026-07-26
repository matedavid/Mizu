#include "render_passes/shadow_render_pass.h"

#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

#include "render_core/rhi/render_pass.h"

#include "registries/light_registry.h"
#include "registries/render_view_registry.h"
#include "render.pipeline/scene_renderer_shaders.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/scene/draw_list_system.h"
#include "render/scene/scene_blackboard_data.h"

namespace Mizu
{

class CascadedShadowMappingRasterPass : public FixedShaderRasterPass
{
  public:
    CascadedShadowMappingRasterPass() : FixedShaderRasterPass(CascadedShadowMappingVS{}, CascadedShadowMappingFS{}) {}
};

MIZU_IMPLEMENT_DRAW_LIST_RASTER_PASS(CascadedShadowMappingRasterPass);

struct GpuCascadedShadowMappingInfo
{
    uint32_t num_cascades;
    uint32_t num_lights;
};

void CascadedShadowModule::update_view(
    uint32_t view_id,
    const RenderViewRegistryEntry& view,
    const ResolvedViewRenderSettings& settings)
{
    ViewData& data = m_view_data[view_id];

    data.cascade_splits.clear();
    data.cascade_light_space_matrices.clear();
    data.num_shadow_casting_directional_lights = 0;

    const ShadowRenderSettings& shadow_settings = settings.resolve<ShadowRenderSettings>();
    data.num_cascades = shadow_settings.num_cascades;

    const glm::mat4 inverse_view_proj = glm::inverse(view.view_proj_matrix);

    const float znear = view.camera.znear;
    const float zfar = view.camera.zfar;
    const float clip_range = zfar - znear;

    for (uint32_t cascade_idx = 0; cascade_idx < shadow_settings.num_cascades; ++cascade_idx)
    {
        const float cascade_split = (znear + shadow_settings.cascade_split_factors[cascade_idx] * clip_range) * -1.0f;
        data.cascade_splits.push_back(cascade_split);
    }

    const std::span<const GpuDirectionalLight> directional_lights = light_registry_get().get_directional_lights();

    for (const GpuDirectionalLight& light : directional_lights)
    {
        if (light.cast_shadows == 0.0f)
            continue;

        data.num_shadow_casting_directional_lights += 1;

        for (uint32_t cascade_idx = 0; cascade_idx < shadow_settings.num_cascades; ++cascade_idx)
        {
            const float split_dist = shadow_settings.cascade_split_factors[cascade_idx];
            const float last_split_dist =
                cascade_idx == 0 ? 0.0f : shadow_settings.cascade_split_factors[cascade_idx - 1];

            glm::vec3 frustum_corners[8] = {
                glm::vec3(-1.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, -1.0f, 0.0f),

                glm::vec3(-1.0f, 1.0f, 1.0f),
                glm::vec3(1.0f, 1.0f, 1.0f),
                glm::vec3(1.0f, -1.0f, 1.0f),
                glm::vec3(-1.0f, -1.0f, 1.0f),
            };

            for (glm::vec3& corner : frustum_corners)
            {
                const glm::vec4 inverted_corner = inverse_view_proj * glm::vec4(corner, 1.0f);
                corner = inverted_corner / inverted_corner.w;
            }

            for (uint32_t i = 0; i < 4; ++i)
            {
                const glm::vec3 dist = frustum_corners[i + 4] - frustum_corners[i];
                frustum_corners[i + 4] = frustum_corners[i] + (dist * split_dist);
                frustum_corners[i] = frustum_corners[i] + (dist * last_split_dist);
            }

            glm::vec3 frustum_center{0.0f};

            for (const glm::vec3& corner : frustum_corners)
            {
                frustum_center += corner;
            }
            frustum_center /= 8.0f;

            float radius = 0.0f;
            for (const glm::vec3& corner : frustum_corners)
            {
                radius = glm::max(radius, glm::length(corner - frustum_center));
            }
            radius = glm::ceil(radius * 16.0f) / 16.0f;

            const glm::vec3 max_extents = glm::vec3(radius);
            const glm::vec3 min_extents = -max_extents;
            const glm::vec3 cascade_extents = max_extents - min_extents;
            const glm::vec3 camera_pos = frustum_center - light.direction * -min_extents.z;

            const glm::mat4 light_view = glm::lookAt(camera_pos, frustum_center, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 light_proj =
                glm::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.0f, cascade_extents.z);

            data.cascade_light_space_matrices.push_back(light_proj * light_view);
        }
    }
}

CascadedShadowData create_cascaded_shadow_data(
    RenderGraphBuilder& builder,
    FrameLinearAllocator& frame_allocator,
    const CascadedShadowModule::ViewData& view_shadows,
    const ShadowRenderSettings& settings)
{
    const uint32_t num_shadow_casting_lights = view_shadows.num_shadow_casting_directional_lights;

    const FrameAllocation cascade_splits_allocation =
        frame_allocator.allocate_structured<float>(view_shadows.cascade_splits.size());
    cascade_splits_allocation.upload(view_shadows.cascade_splits);

    const FrameAllocation cascade_matrices_allocation =
        frame_allocator.allocate_structured<glm::mat4>(view_shadows.cascade_light_space_matrices.size());
    cascade_matrices_allocation.upload(view_shadows.cascade_light_space_matrices);

    const uint32_t width = std::max(settings.resolution * settings.num_cascades, 1u);
    const uint32_t height = std::max(settings.resolution * num_shadow_casting_lights, 1u);

    const RenderGraphResource shadow_atlas =
        builder.create_texture2d(width, height, ImageFormat::D32_SFLOAT, "CascadedShadowAtlas");

    return CascadedShadowData{
        .cascaded_shadow_atlas = shadow_atlas,
        .cascade_splits_allocation = cascade_splits_allocation,
        .cascade_light_space_matrices_allocation = cascade_matrices_allocation,
        .num_shadow_casting_directional_lights = num_shadow_casting_lights,
    };
}

void add_cascaded_shadow_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    const CascadedShadowData& cascaded_data = blackboard.get<CascadedShadowData>();
    const RenderViewData& view_data = blackboard.get<RenderViewData>();
    const ShadowRenderSettings& settings = view_data.render_settings.resolve<ShadowRenderSettings>();

    if (cascaded_data.num_shadow_casting_directional_lights == 0)
        return;

    const uint32_t num_cascades = settings.num_cascades;
    const uint32_t num_lights = cascaded_data.num_shadow_casting_directional_lights;

    const uint32_t width = settings.resolution * num_cascades;
    const uint32_t height = settings.resolution * num_lights;

    struct CascadedShadowPassData
    {
        RenderGraphResource shadow_map_texture;
        FrameAllocation shadow_mapping_info;
        DrawListHandle draw_list_handle;
    };

    builder.add_pass<CascadedShadowPassData>(
        "CascadedShadowMapping",
        [&](RenderGraphPassBuilder& pass, CascadedShadowPassData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);

            const RenderSystemsData& systems_data = blackboard.get<RenderSystemsData>();

            const FrameAllocation shadow_mapping_allocation =
                systems_data.frame_allocator.allocate_constant<GpuCascadedShadowMappingInfo>();
            shadow_mapping_allocation.upload<GpuCascadedShadowMappingInfo>({
                .num_cascades = num_cascades,
                .num_lights = num_lights,
            });

            data.shadow_map_texture = pass.attachment(cascaded_data.cascaded_shadow_atlas);
            data.shadow_mapping_info = shadow_mapping_allocation;
            data.draw_list_handle = create_draw_list({
                .raster_pass = get_CascadedShadowMappingRasterPass(),
            });
        },
        [=](CommandBuffer& command, const CascadedShadowPassData& data, const RenderGraphPassResources& resources) {
            RenderPassInfo render_pass{};
            render_pass.extent = {width, height};
            render_pass.depth_stencil_attachment = resources.get_framebuffer_attachment(
                data.shadow_map_texture, LoadOperation::Clear, StoreOperation::Store, glm::vec4(1.0f));

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(CascadedShadowMappingPassLayout)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Vertex)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Vertex)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::StructuredBufferSrv(0, cascaded_data.cascade_light_space_matrices_allocation.view),
                WriteDescriptor::ConstantBuffer(0, data.shadow_mapping_info.view),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                CascadedShadowMappingPassLayout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            command.begin_render_pass(render_pass);
            {
                const DrawListRasterPassInfo raster_pass_info{
                    .rasterization_state =
                        RasterizationState{
                            .depth_clamp = true,
                            .cull_mode = RasterizationState::CullMode::Front,
                        },
                    .depth_stencil_state =
                        DepthStencilState{
                            .depth_test = true,
                            .depth_write = true,
                        },
                    .framebuffer_info = create_framebuffer_info(render_pass),
                    .bindings = DrawListRasterBindings{}.add(1, descriptor_set),
                };

                const uint32_t view_count = num_cascades * num_lights;
                dispatch_draw_list(command, data.draw_list_handle, raster_pass_info, view_count);
            }
            command.end_render_pass();
        });
}

} // namespace Mizu
