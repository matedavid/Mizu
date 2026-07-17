#include "render/render_graph_renderer.h"

#include "base/debug/profiling.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/rhi_helpers.h"
#include "render_core/rhi/sampler_state.h"

#include "registries/light_registry.h"
#include "render.pipeline/render_graph_renderer_shaders.h"
#include "render/core/camera.h"
#include "render/passes/pass_info.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/scene/draw_list_system.h"
#include "render/state_manager/camera_state_manager.h"
#include "render/state_manager/renderer_settings_state_manager.h"
#include "render/systems/frame_linear_allocator.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "render/utils/buffer_utils.h"
#include "resources/gpu_pools.h"
#include "resources/residency_system.h"

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

struct LightsInfo
{
    FrameAllocation point_lights_view;
    FrameAllocation directional_lights_view;

    FrameAllocation cascade_splits_view;
    FrameAllocation cascade_light_space_matrices_view;
    uint32_t num_shadow_casting_directional_lights = 0;
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
};

RenderGraphRenderer::RenderGraphRenderer()
{
    struct FullscreenTriangleVertex
    {
        glm::vec3 position;
        glm::vec2 texCoord;
    };

    std::array<FullscreenTriangleVertex, 3> vertex_data;

    if (g_render_device->get_api() == GraphicsApi::Dx12)
    {
        // clang-format off
        vertex_data = {
            FullscreenTriangleVertex{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
            FullscreenTriangleVertex{{ 3.0f, -1.0f, 0.0f}, {2.0f, 1.0f}},
            FullscreenTriangleVertex{{-1.0f,  3.0f, 0.0f}, {0.0f, -1.0f}},
        };
        // clang-format on
    }
    else if (g_render_device->get_api() == GraphicsApi::Vulkan)
    {
        // clang-format off
        vertex_data = {
            FullscreenTriangleVertex{{-1.0f,  1.0f, 0.0f}, {0.0f, 1.0f}},
            FullscreenTriangleVertex{{ 3.0f,  1.0f, 0.0f}, {2.0f, 1.0f}},
            FullscreenTriangleVertex{{-1.0f, -3.0f, 0.0f}, {0.0f, -1.0f}},
        };
        // clang-format on
    }

    m_fullscreen_triangle = BufferUtils::create_vertex_buffer(
        std::span<const FullscreenTriangleVertex>(vertex_data), "TriangleVertexBuffer");
}

void RenderGraphRenderer::set_render_module_systems(const RenderModuleSystems& systems)
{
    m_frame_allocator = systems.frame_allocator;
    m_texture_residency_system = systems.texture_residency_system;
    m_material_residency_system = systems.material_residency_system;
}

void RenderGraphRenderer::build_render_graph(
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard,
    const RenderModuleFrameData& frame_data)
{
    MIZU_PROFILE_SCOPED;

    (void)frame_data;

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

    const FrameAllocation camera_info = m_frame_allocator->allocate_constant<GPUCameraInfo>();
    camera_info.upload(gpu_camera_info);

    RenderGraphRendererFrameInfo& rgr_frame_info = blackboard.add<RenderGraphRendererFrameInfo>();
    rgr_frame_info.width = frame_info.width;
    rgr_frame_info.height = frame_info.height;
    rgr_frame_info.camera_info = gpu_camera_info;
    rgr_frame_info.camera_info_view = camera_info;
    rgr_frame_info.output_texture = frame_info.output_texture_ref;

    get_light_information(blackboard);

    render_scene(builder, blackboard);
}

void RenderGraphRenderer::render_scene(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
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

class DepthNormalsRasterPass : public FixedShaderRasterPass
{
  public:
    DepthNormalsRasterPass() : FixedShaderRasterPass(DepthNormalsPrepassShaderVS{}, DepthNormalsPrepassShaderFS{}) {}
};

MIZU_IMPLEMENT_DRAW_LIST_RASTER_PASS(DepthNormalsRasterPass);

void RenderGraphRenderer::add_depth_normals_prepass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
    const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();

    const RenderGraphResource depth_texture =
        builder.create_texture2d(frame_info.width, frame_info.height, ImageFormat::D32_SFLOAT, "DepthTexture");

    struct DepthNormalsData
    {
        RenderGraphResource depth_texture;
        DrawListHandle draw_list_handle;
    };

    builder.add_pass<DepthNormalsData>(
        "DepthNormalsPrepass",
        [&](RenderGraphPassBuilder& pass, DepthNormalsData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);

            data.depth_texture = pass.attachment(depth_texture);
            data.draw_list_handle = create_draw_list({
                .raster_pass = get_DepthNormalsRasterPass(),
                .frustum = Frustum::from_view_projection(frame_info.camera_info.viewProj, frame_info.camera_info.pos),
            });
        },
        [=](CommandBuffer& command, const DepthNormalsData& data, const RenderGraphPassResources& resources) {
            FramebufferAttachment depth_attachment{};
            depth_attachment.rtv = ImageResourceView::create(resources.get_image(data.depth_texture));
            depth_attachment.load_operation = LoadOperation::Clear;
            depth_attachment.store_operation = StoreOperation::Store;
            depth_attachment.clear_value = glm::vec4(1.0f);

            RenderPassInfo pass_info{};
            pass_info.extent = {frame_info.width, frame_info.height};
            pass_info.depth_stencil_attachment = depth_attachment;

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(DepthNormalsLayout)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Vertex)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::ConstantBuffer(0, frame_info.camera_info_view.view),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                DepthNormalsLayout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            DepthStencilState depth_stencil{};
            depth_stencil.depth_test = true;
            depth_stencil.depth_write = true;

            FramebufferInfo framebuffer_info{};
            framebuffer_info.depth_stencil_attachment = resources.get_image(data.depth_texture)->get_format();

            DrawListRasterBindings bindings{};
            bindings.add(1, descriptor_set);

            command.begin_render_pass(pass_info);
            {
                const DrawListRasterPassInfo raster_pass_info{
                    .depth_stencil_state = depth_stencil,
                    .framebuffer_info = framebuffer_info,
                    .bindings = bindings,
                };

                dispatch_draw_list(command, data.draw_list_handle, raster_pass_info);
            }
            command.end_render_pass();
        });

    DepthNormalsPrepassInfo& depth_normals_prepass_info = blackboard.add<DepthNormalsPrepassInfo>();
    depth_normals_prepass_info.depth_texture = depth_texture;
}

void RenderGraphRenderer::add_light_culling_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    const LightsInfo& lights_info = blackboard.get<LightsInfo>();
    const DepthNormalsPrepassInfo& depth_normals_info = blackboard.get<DepthNormalsPrepassInfo>();

    // Should match values defined in LightCullingCommon.slang
    constexpr uint32_t TILE_SIZE = LightCullingShaderCS2::TILE_SIZE;
    constexpr uint32_t MAX_LIGHTS_PER_TILE = LightCullingShaderCS2::MAX_LIGHTS_PER_TILE;

    const glm::uvec3 group_count =
        compute_group_count({frame_info.width, frame_info.height, 1.0f}, {TILE_SIZE, TILE_SIZE, 1.0f});

    const uint32_t num_tiles = group_count.x * group_count.y;
    const uint32_t point_lights_number = num_tiles * MAX_LIGHTS_PER_TILE;

    const RenderGraphResource visible_point_light_indices =
        builder.create_structured_buffer<uint32_t>(point_lights_number, "VisiblePointLightIndices");

    GpuLightCullingInfo gpu_light_culling_info{};
    gpu_light_culling_info.num_tiles = glm::uvec2(group_count);

    const FrameAllocation light_culling_info = m_frame_allocator->allocate_constant<GpuLightCullingInfo>();
    light_culling_info.upload(gpu_light_culling_info);

    struct LightCullingData
    {
        RenderGraphResource visible_point_light_indices;
        RenderGraphResource depth_texture;
        FrameAllocation light_culling_info;
    };

    builder.add_pass<LightCullingData>(
        "LightCulling",
        [&](RenderGraphPassBuilder& pass, LightCullingData& data) {
            pass.set_hint(RenderGraphPassHint::Compute);

            data.visible_point_light_indices = pass.write(visible_point_light_indices);
            data.depth_texture = pass.read(depth_normals_info.depth_texture);
            data.light_culling_info = light_culling_info;
        },
        [=](CommandBuffer& command, const LightCullingData& data, const RenderGraphPassResources& resources) {
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

            const auto pipeline = get_compute_pipeline(LightCullingShaderCS2{});
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

class CascadedShadowMappingRasterPass2 : public FixedShaderRasterPass
{
  public:
    CascadedShadowMappingRasterPass2()
        : FixedShaderRasterPass(CascadedShadowMappingShaderVS{}, CascadedShadowMappingShaderFS{})
    {
    }
};

MIZU_IMPLEMENT_DRAW_LIST_RASTER_PASS(CascadedShadowMappingRasterPass2);

void RenderGraphRenderer::add_cascaded_shadow_mapping_pass(
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const CascadedShadowsSettings& shadow_settings = blackboard.get<RenderGraphRendererSettings>().cascaded_shadows;
    const LightsInfo& lights_info = blackboard.get<LightsInfo>();

    const uint32_t num_shadow_casting_directional_lights = lights_info.num_shadow_casting_directional_lights;

    const uint32_t width = std::max(shadow_settings.resolution * shadow_settings.num_cascades, 1u);
    const uint32_t height = std::max(shadow_settings.resolution * num_shadow_casting_directional_lights, 1u);

    const RenderGraphResource shadow_map_texture =
        builder.create_texture2d(width, height, ImageFormat::D32_SFLOAT, "ShadowMapTexture");

    struct CascadedShadowMappingInfo
    {
        uint32_t num_cascades;
        uint32_t num_lights;
    };

    FrameAllocation shadow_mapping_allocation = m_frame_allocator->allocate_constant<CascadedShadowMappingInfo>();
    shadow_mapping_allocation.upload<CascadedShadowMappingInfo>({
        .num_cascades = shadow_settings.num_cascades,
        .num_lights = num_shadow_casting_directional_lights,
    });

    struct CascadedShadowMappingData
    {
        RenderGraphResource shadow_map_texture;
        FrameAllocation shadow_mapping_info;
        DrawListHandle draw_list_handle;
    };

    builder.add_pass<CascadedShadowMappingData>(
        "CascadedShadowMapping",
        [&](RenderGraphPassBuilder& pass, CascadedShadowMappingData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);

            data.shadow_map_texture = pass.attachment(shadow_map_texture);
            data.shadow_mapping_info = shadow_mapping_allocation;
            data.draw_list_handle = create_draw_list({
                .raster_pass = get_CascadedShadowMappingRasterPass2(),
            });
        },
        [=](CommandBuffer& command, const CascadedShadowMappingData& data, const RenderGraphPassResources& resources) {
            FramebufferAttachment depth_attachment{};
            depth_attachment.rtv = ImageResourceView::create(resources.get_image(data.shadow_map_texture));
            depth_attachment.load_operation = LoadOperation::Clear;
            depth_attachment.store_operation = StoreOperation::Store;
            depth_attachment.clear_value = glm::vec4(1.0f);

            RenderPassInfo pass_info{};
            pass_info.extent = {width, height};
            pass_info.depth_stencil_attachment = depth_attachment;

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(CascadedShadowMappingLayout)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Vertex) 
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Vertex)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::StructuredBufferSrv(0, lights_info.cascade_light_space_matrices_view.view),
                WriteDescriptor::ConstantBuffer(0, data.shadow_mapping_info.view),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                CascadedShadowMappingLayout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            RasterizationState raster{};
            raster.depth_clamp = true;
            raster.cull_mode = RasterizationState::CullMode::Front;

            DepthStencilState depth_stencil{};
            depth_stencil.depth_test = true;
            depth_stencil.depth_write = true;

            FramebufferInfo framebuffer_info{};
            framebuffer_info.depth_stencil_attachment = ImageFormat::D32_SFLOAT;

            DrawListRasterBindings bindings{};
            bindings.add(1, descriptor_set);

            command.begin_render_pass(pass_info);
            {
                const DrawListRasterPassInfo raster_pass_info{
                    .rasterization_state = raster,
                    .depth_stencil_state = depth_stencil,
                    .framebuffer_info = framebuffer_info,
                    .bindings = bindings,
                };

                const uint32_t view_count = shadow_settings.num_cascades * num_shadow_casting_directional_lights;
                dispatch_draw_list(command, data.draw_list_handle, raster_pass_info, view_count);
            }
            command.end_render_pass();
        });

    ShadowsInfo& shadows_info = blackboard.add<ShadowsInfo>();
    shadows_info.shadow_map_texture = shadow_map_texture;
}

class LightingRasterPass : public MaterialShaderRasterPass
{
};

MIZU_IMPLEMENT_DRAW_LIST_RASTER_PASS(LightingRasterPass);

void RenderGraphRenderer::add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
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
        DrawListHandle draw_list_handle;
    };

    builder.add_pass<LightingData>(
        "Lighting",
        [&](RenderGraphPassBuilder& pass, LightingData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);

            data.output_texture = pass.attachment(frame_info.output_texture);
            data.depth_texture = pass.attachment(depth_normals_info.depth_texture);
            data.visible_point_light_indices = pass.read(culling_info.visible_point_light_indices);
            data.directional_shadow_map = pass.read(shadows_info.shadow_map_texture);
            data.draw_list_handle = create_draw_list({
                .raster_pass = get_LightingRasterPass(),
                .frustum = Frustum::from_view_projection(frame_info.camera_info.viewProj, frame_info.camera_info.pos),
            });
        },
        [=, this](CommandBuffer& command, const LightingData& data, const RenderGraphPassResources& resources) {
            ImageResourceViewDescription output_view_desc{};
            output_view_desc.override_format = ImageFormat::R8G8B8A8_SRGB;

            FramebufferAttachment color_attachment{};
            color_attachment.rtv =
                ImageResourceView::create(resources.get_image(data.output_texture), output_view_desc);
            color_attachment.load_operation = LoadOperation::Clear;
            color_attachment.store_operation = StoreOperation::Store;
            color_attachment.clear_value = glm::vec4(0.0f);

            FramebufferAttachment depth_attachment{};
            depth_attachment.rtv = ImageResourceView::create(resources.get_image(data.depth_texture));
            depth_attachment.load_operation = LoadOperation::Load;
            depth_attachment.store_operation = StoreOperation::Store;

            RenderPassInfo pass_info{};
            pass_info.extent = {frame_info.width, frame_info.height};
            pass_info.color_attachments = {color_attachment};
            pass_info.depth_stencil_attachment = depth_attachment;

            FramebufferInfo framebuffer_info{};
            framebuffer_info.color_attachments = {*output_view_desc.override_format};
            framebuffer_info.depth_stencil_attachment = resources.get_image(data.depth_texture)->get_format();

            // clang-format off
            MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(LightingLayout)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Vertex | ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(1, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(2, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(1, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(3, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(0, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(4, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(5, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(6, 1, ShaderType::Fragment)
                MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(1, 1, ShaderType::Fragment)
            MIZU_END_DESCRIPTOR_SET_LAYOUT()
            // clang-format on

            const std::array writes = {
                WriteDescriptor::ConstantBuffer(0, frame_info.camera_info_view.view),
                WriteDescriptor::StructuredBufferSrv(0, lights_info.point_lights_view.view),
                WriteDescriptor::StructuredBufferSrv(1, lights_info.directional_lights_view.view),
                WriteDescriptor::StructuredBufferSrv(
                    2, BufferResourceView::create(resources.get_buffer(data.visible_point_light_indices))),
                WriteDescriptor::ConstantBuffer(1, culling_info.light_culling_info.view),
                WriteDescriptor::TextureSrv(
                    3, ImageResourceView::create(resources.get_image(data.directional_shadow_map))),
                WriteDescriptor::SamplerState(0, directional_shadow_map_sampler),
                WriteDescriptor::StructuredBufferSrv(4, lights_info.cascade_splits_view.view),
                WriteDescriptor::StructuredBufferSrv(5, lights_info.cascade_light_space_matrices_view.view),
                WriteDescriptor::StructuredBufferSrv(
                    6, BufferResourceView::create(m_material_residency_system->get_material_buffer())),
                WriteDescriptor::SamplerState(1, get_sampler_state({})),
            };

            const auto descriptor_set = g_render_device->allocate_descriptor_set(
                LightingLayout::get_layout(), DescriptorSetAllocationType::Transient);
            descriptor_set->update(writes);

            DepthStencilState depth_stencil{};
            depth_stencil.depth_test = true;
            depth_stencil.depth_write = false;
            depth_stencil.depth_compare_op = DepthStencilState::DepthCompareOp::LessEqual;

            DrawListRasterBindings bindings{};
            bindings.add(1, descriptor_set);
            bindings.add(2, m_texture_residency_system->get_bindless_descriptor_set());

            command.begin_render_pass(pass_info);
            {
                const DrawListRasterPassInfo raster_pass_info{
                    .depth_stencil_state = depth_stencil,
                    .framebuffer_info = framebuffer_info,
                    .bindings = bindings,
                };

                dispatch_draw_list(command, data.draw_list_handle, raster_pass_info);
            }
            command.end_render_pass();
        });
}

void RenderGraphRenderer::add_light_culling_debug_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
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
        [&](RenderGraphPassBuilder& pass, LightCullingDebugData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);

            data.output_texture = pass.attachment(frame_info.output_texture);
            data.visible_point_light_indices = pass.read(culling_info.visible_point_light_indices);
        },
        [=,
         this](CommandBuffer& command, const LightCullingDebugData& data, const RenderGraphPassResources& resources) {
            ImageResourceViewDescription output_view_desc{};
            output_view_desc.override_format = ImageFormat::R8G8B8A8_SRGB;

            FramebufferAttachment color_attachment{};
            color_attachment.rtv =
                ImageResourceView::create(resources.get_image(data.output_texture), output_view_desc);
            color_attachment.load_operation = LoadOperation::Load;
            color_attachment.store_operation = StoreOperation::Store;

            RenderPassInfo pass_info{};
            pass_info.extent = {frame_info.width, frame_info.height};
            pass_info.color_attachments = {color_attachment};

            FramebufferInfo framebuffer_info{};
            framebuffer_info.color_attachments = {*output_view_desc.override_format};

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
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    const DepthNormalsPrepassInfo& depth_normals_info = blackboard.get<DepthNormalsPrepassInfo>();
    const ShadowsInfo& shadows_info = blackboard.get<ShadowsInfo>();
    const LightsInfo& lights_info = blackboard.get<LightsInfo>();

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
        [&](RenderGraphPassBuilder& pass, DrawCascadesData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);
            data.output_texture = pass.attachment(frame_info.output_texture);
            data.depth_texture = pass.read(depth_normals_info.depth_texture);
        },
        [=, this](CommandBuffer& command, const DrawCascadesData& data, const RenderGraphPassResources& resources) {
            ImageResourceViewDescription output_view_desc{};
            output_view_desc.override_format = ImageFormat::R8G8B8A8_SRGB;

            FramebufferAttachment color_attachment{};
            color_attachment.rtv =
                ImageResourceView::create(resources.get_image(data.output_texture), output_view_desc);
            color_attachment.load_operation = LoadOperation::Load;
            color_attachment.store_operation = StoreOperation::Store;

            RenderPassInfo pass_info{};
            pass_info.extent = {frame_info.width, frame_info.height};
            pass_info.color_attachments = {color_attachment};

            FramebufferInfo framebuffer_info{};
            framebuffer_info.color_attachments = {*output_view_desc.override_format};

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
                WriteDescriptor::StructuredBufferSrv(0, lights_info.cascade_splits_view.view),
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
        [&](RenderGraphPassBuilder& pass, DrawShadowMapData& data) {
            pass.set_hint(RenderGraphPassHint::Raster);
            data.output_texture = pass.attachment(frame_info.output_texture);
            data.shadow_map_texture = pass.read(shadows_info.shadow_map_texture);
        },
        [=, this](CommandBuffer& command, const DrawShadowMapData& data, const RenderGraphPassResources& resources) {
            FramebufferAttachment color_attachment{};
            color_attachment.rtv = ImageResourceView::create(resources.get_image(data.output_texture));
            color_attachment.load_operation = LoadOperation::Load;
            color_attachment.store_operation = StoreOperation::Store;

            RenderPassInfo pass_info{};
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

    FrameLinearAllocator& frame_allocator = *m_frame_allocator;
    const LightRegistry& light_registry = light_registry_get();

    const FrameAllocation point_lights =
        frame_allocator.allocate_structured<GpuPointLight>(light_registry.get_point_lights().size());
    point_lights.upload(light_registry.get_point_lights());

    const FrameAllocation directional_lights =
        frame_allocator.allocate_structured<GpuDirectionalLight>(light_registry.get_directional_lights().size());
    directional_lights.upload(light_registry.get_directional_lights());

    const FrameAllocation cascade_splits =
        frame_allocator.allocate_structured<float>(light_registry.get_cascade_splits().size());
    cascade_splits.upload(light_registry.get_cascade_splits());

    const FrameAllocation cascade_light_space_matrices =
        frame_allocator.allocate_structured<glm::mat4>(light_registry.get_cascade_light_space_matrices().size());
    cascade_light_space_matrices.upload(light_registry.get_cascade_light_space_matrices());

    LightsInfo& lights_info = blackboard.add<LightsInfo>();
    lights_info.point_lights_view = point_lights;
    lights_info.directional_lights_view = directional_lights;
    lights_info.cascade_splits_view = cascade_splits;
    lights_info.cascade_light_space_matrices_view = cascade_light_space_matrices;
    lights_info.num_shadow_casting_directional_lights = light_registry.get_num_shadow_casting_directional_lights();
}

} // namespace Mizu
