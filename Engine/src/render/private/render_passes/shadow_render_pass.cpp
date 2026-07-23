#include "render_passes/shadow_render_pass.h"

#include "render_core/rhi/render_pass.h"

#include "registries/light_registry.h"
#include "registries/render_settings_registry.h"
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

CascadedShadowData create_cascaded_shadow_data(RenderGraphBuilder& builder, FrameLinearAllocator& frame_allocator)
{
    const LightRegistry& light_registry = light_registry_get();
    const ShadowRenderSettings& settings = render_settings_registry_resolve<ShadowRenderSettings>();

    const uint32_t num_shadow_casting_lights = light_registry.get_num_shadow_casting_directional_lights();

    const std::span<const float> cascade_splits = light_registry.get_cascade_splits();
    const std::span<const glm::mat4> cascade_light_space_matrices = light_registry.get_cascade_light_space_matrices();

    const FrameAllocation cascade_splits_allocation = frame_allocator.allocate_structured<float>(cascade_splits.size());
    cascade_splits_allocation.upload(cascade_splits);

    const FrameAllocation cascade_matrices_allocation =
        frame_allocator.allocate_structured<glm::mat4>(cascade_light_space_matrices.size());
    cascade_matrices_allocation.upload(cascade_light_space_matrices);

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
    const ShadowRenderSettings& settings = render_settings_registry_resolve<ShadowRenderSettings>();

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