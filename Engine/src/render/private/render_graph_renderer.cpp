#include "render/render_graph_renderer.h"

#include <format>
#include <glm/gtc/matrix_transform.hpp>

#include "base/debug/profiling.h"
#include "core/thread_sync.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/sampler_state.h"

#include "render.pipeline/render_graph_renderer_shaders.h"
#include "render/camera.h"
#include "render/core/buffer_utils.h"
#include "render/core/render_utils.h"
#include "render/material/material.h"
#include "render/passes/pass_info.h"
#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/render_graph/render_graph_utils.h"
#include "render/runtime/renderer.h"
#include "render/state_manager/camera_state_manager.h"
#include "render/state_manager/light_state_manager.h"
#include "render/state_manager/renderer_settings_state_manager.h"
#include "render/state_manager/static_mesh_state_manager.h"
#include "render/state_manager/transform_state_manager.h"
#include "render/systems/pipeline_cache.h"
#include "render/systems/sampler_state_cache.h"
#include "render_graph_renderer_parameters.h"

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

struct GPULightCullingInfo
{
    glm::uvec2 num_tiles;
};

struct RenderGraphRendererFrameInfo
{
    uint32_t width, height;
    GPUCameraInfo camera_info;
    RGBufferCbvRef camera_info_ref;
    RGTextureRtvRef output_view_ref;
};

struct DrawInfo
{
    DrawListHandle main_view_handle;
    DrawListHandle cascaded_shadows_handle;

    RGBufferSrvRef transform_info_ref;
    RGBufferSrvRef main_view_indices_ref;
    RGBufferSrvRef cascaded_shadows_indices_ref;
};

struct LightsInfo
{
    RGBufferSrvRef point_lights_ref;
    RGBufferSrvRef directional_lights_ref;
};

struct DepthNormalsPrepassInfo
{
    RGTextureSrvRef depth_view_srv_ref;
    RGTextureRtvRef depth_view_rtv_ref;
    // RGTextureSrvRef normals_view_ref;
};

struct LightCullingInfo
{
    RGBufferSrvRef visible_point_light_indices_ref;
    RGBufferCbvRef light_culling_info_ref;
};

struct ShadowsInfo
{
    RGTextureSrvRef shadow_map_view_ref;
    RGBufferSrvRef cascade_splits_ref;
    RGBufferSrvRef light_space_matrices_ref;
};

RenderGraphRenderer::RenderGraphRenderer()
{
    struct FullscreenTriangleVertex
    {
        glm::vec3 position;
        glm::vec2 texCoord;
    };

    const std::vector<FullscreenTriangleVertex> vertices = {
        {{3.0f, -1.0f, 0.0f}, {2.0f, 0.0f}},
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
        {{-1.0f, 3.0f, 0.0f}, {0.0f, 2.0f}},
    };

    BufferDescription fullscreen_triangle_desc{};
    fullscreen_triangle_desc.size = sizeof(FullscreenTriangleVertex) * vertices.size();
    fullscreen_triangle_desc.stride = sizeof(FullscreenTriangleVertex);
    fullscreen_triangle_desc.usage = BufferUsageBits::VertexBuffer | BufferUsageBits::TransferDst;
    fullscreen_triangle_desc.name = "Fullscreen Triangle";

    m_fullscreen_triangle = g_render_device->create_buffer(fullscreen_triangle_desc);
    BufferUtils::initialize_buffer(
        *m_fullscreen_triangle, reinterpret_cast<const uint8_t*>(vertices.data()), fullscreen_triangle_desc.size);

    m_draw_manager = std::make_unique<DrawBlockManager>();

    m_transform_info_buffer.resize(TRANSFORM_INFO_BUFFER_SIZE);
    m_main_view_transform_indices_buffer.resize(TRANSFORM_INFO_BUFFER_SIZE);
    m_cascaded_shadows_transform_indices_buffer.resize(TRANSFORM_INFO_BUFFER_SIZE);
}

void RenderGraphRenderer::build_render_graph(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    MIZU_PROFILE_SCOPED;

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

    const RGBufferRef camera_info_ref = builder.create_constant_buffer(gpu_camera_info, "CameraInfo");

    ImageResourceViewDescription output_view_desc{};
    output_view_desc.override_format = ImageFormat::R8G8B8A8_SRGB;
    const RGTextureRtvRef output_view_ref = builder.create_texture_rtv(frame_info.output_texture_ref, output_view_desc);

    RenderGraphRendererFrameInfo& rgr_frame_info = blackboard.add<RenderGraphRendererFrameInfo>();
    rgr_frame_info.width = frame_info.width;
    rgr_frame_info.height = frame_info.height;
    rgr_frame_info.camera_info = gpu_camera_info;
    rgr_frame_info.camera_info_ref = builder.create_buffer_cbv(camera_info_ref);
    rgr_frame_info.output_view_ref = output_view_ref;

    m_draw_manager->reset();

    get_light_information(builder, blackboard);
    create_draw_lists(builder, blackboard);

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

void RenderGraphRenderer::add_depth_normals_prepass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
    const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    const DrawInfo& draw_info = blackboard.get<DrawInfo>();

    // const RGImageRef normals_texture_ref = builder.create_texture<Texture2D>(
    //     {frame_info.width, frame_info.height}, ImageFormat::R32G32B32A32_SFLOAT, "NormalsTexture");
    // const RGTextureRtvRef normals_view_rtv_ref = builder.create_texture_rtv(normals_texture_ref);

    const RGImageRef depth_texture_ref =
        builder.create_texture({frame_info.width, frame_info.height}, ImageFormat::D32_SFLOAT, "DepthTexture");
    const RGTextureRtvRef depth_view_rtv_ref = builder.create_texture_rtv(depth_texture_ref);

    DepthNormalsPrepassParameters params{};
    params.cameraInfo = frame_info.camera_info_ref;
    params.transformIndices = draw_info.main_view_indices_ref;
    params.transformInfo = draw_info.transform_info_ref;
    params.framebuffer = RGFramebufferAttachments{
        .width = frame_info.width,
        .height = frame_info.height,
        // .color_attachments = {normals_view_rtv_ref},
        .depth_stencil_attachment = depth_view_rtv_ref,
    };

    RGResourceGroupLayout layout0{};
    layout0.add_resource(0, params.cameraInfo, ShaderType::Vertex);
    layout0.add_resource(1, params.transformInfo, ShaderType::Vertex);
    layout0.add_resource(2, params.transformIndices, ShaderType::Vertex);

    const RGResourceGroupRef resource_group0_ref = builder.create_resource_group(layout0);

    DepthNormalsPrepassShaderVS vertex_shader;
    DepthNormalsPrepassShaderFS fragment_shader;

    DepthStencilState depth_stencil{};
    depth_stencil.depth_test = true;
    depth_stencil.depth_write = true;

    builder.add_pass(
        "DepthNormalsPrepass",
        params,
        RGPassHint::Raster,
        [=, this](CommandBuffer& command, const RGPassResources& resources) {
            const auto framebuffer = resources.get_framebuffer();
            command.begin_render_pass(framebuffer);
            {
                const auto pipeline = get_graphics_pipeline(
                    vertex_shader,
                    fragment_shader,
                    RasterizationState{},
                    depth_stencil,
                    ColorBlendState{},
                    framebuffer);
                command.bind_pipeline(pipeline);

                bind_resource_group(command, resources, resource_group0_ref, 0);

                struct PushConstant
                {
                    uint64_t transform_offset;
                };

                const DrawList& list = m_draw_manager->get_draw_list(draw_info.main_view_handle);
                for (size_t block_idx = 0; block_idx < list.num_blocks; ++block_idx)
                {
                    const DrawBlock& block = list.blocks[block_idx];

                    const std::string draw_block_name = std::format("DrawBlock:{}", block.pipeline_hash);
                    command.begin_gpu_marker(draw_block_name);

                    for (size_t elem_idx = 0; elem_idx < block.num_elements; ++elem_idx)
                    {
                        const DrawElement& element = block.elements[elem_idx];

                        PushConstant push_constant{};
                        push_constant.transform_offset = element.transform_offset;
                        command.push_constant(push_constant);

                        draw_mesh_instanced(command, *element.mesh, element.instance_count);
                    }

                    command.end_gpu_marker();
                }
            }
            command.end_render_pass();
        });

    DepthNormalsPrepassInfo& depth_normals_prepass_info = blackboard.add<DepthNormalsPrepassInfo>();
    depth_normals_prepass_info.depth_view_srv_ref = builder.create_texture_srv(depth_texture_ref);
    depth_normals_prepass_info.depth_view_rtv_ref = depth_view_rtv_ref;
    // depth_normals_prepass_info.normals_view_ref = builder.create_texture_srv(normals_texture_ref);
}

void RenderGraphRenderer::add_light_culling_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    const LightsInfo& lights_info = blackboard.get<LightsInfo>();
    const DepthNormalsPrepassInfo& depth_normals_info = blackboard.get<DepthNormalsPrepassInfo>();

    // Should match values defined in LightCullingCommon.slang
    constexpr uint32_t TILE_SIZE = LightCullingShaderCS::TILE_SIZE;
    constexpr uint32_t MAX_LIGHTS_PER_TILE = LightCullingShaderCS::MAX_LIGHTS_PER_TILE;

    const glm::uvec3 group_count =
        compute_group_count({frame_info.width, frame_info.height, 1.0f}, {TILE_SIZE, TILE_SIZE, 1.0f});

    const uint32_t num_tiles = group_count.x * group_count.y;
    const uint32_t point_lights_number = num_tiles * MAX_LIGHTS_PER_TILE;

    const RGBufferRef visible_point_light_indices_ref =
        builder.create_structured_buffer<uint32_t>(point_lights_number, "VisiblePointLightIndices");

    GPULightCullingInfo gpu_light_culling_info{};
    gpu_light_culling_info.num_tiles = glm::uvec2(group_count);

    const RGBufferRef light_culling_info_ref =
        builder.create_constant_buffer(gpu_light_culling_info, "LightCullingInfo");

    LightCullingShaderParameters params{};
    params.cameraInfo = frame_info.camera_info_ref;
    params.pointLights = lights_info.point_lights_ref;
    params.visiblePointLightIndices = builder.create_buffer_uav(visible_point_light_indices_ref);
    params.lightCullingInfo = builder.create_buffer_cbv(light_culling_info_ref);
    params.depthTexture = depth_normals_info.depth_view_srv_ref;
    params.depthTextureSampler = get_sampler_state(SamplerStateDescription{});

    RGResourceGroupLayout layout0{};
    layout0.add_resource(0, params.cameraInfo, ShaderType::Compute);

    RGResourceGroupLayout layout1{};
    layout1.add_resource(0, params.pointLights, ShaderType::Compute);
    layout1.add_resource(1, params.visiblePointLightIndices, ShaderType::Compute);
    layout1.add_resource(2, params.lightCullingInfo, ShaderType::Compute);

    RGResourceGroupLayout layout2{};
    layout2.add_resource(0, params.depthTexture, ShaderType::Compute);
    layout2.add_resource(1, params.depthTextureSampler, ShaderType::Compute);

    const RGResourceGroupRef resource_group0_ref = builder.create_resource_group(layout0);
    const RGResourceGroupRef resource_group1_ref = builder.create_resource_group(layout1);
    const RGResourceGroupRef resource_group2_ref = builder.create_resource_group(layout2);

    LightCullingShaderCS shader;

    builder.add_pass(
        "LightCulling", params, RGPassHint::Compute, [=](CommandBuffer& command, const RGPassResources& resources) {
            const auto pipeline = get_compute_pipeline(shader);
            command.bind_pipeline(pipeline);

            bind_resource_group(command, resources, resource_group0_ref, 0);
            bind_resource_group(command, resources, resource_group1_ref, 1);
            bind_resource_group(command, resources, resource_group2_ref, 2);

            command.dispatch(group_count);
        });

    LightCullingInfo& culling_info = blackboard.add<LightCullingInfo>();
    culling_info.visible_point_light_indices_ref = builder.create_buffer_srv(visible_point_light_indices_ref);
    culling_info.light_culling_info_ref = params.lightCullingInfo;
}

void RenderGraphRenderer::add_cascaded_shadow_mapping_pass(
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const CascadedShadowsSettings& shadow_settings = blackboard.get<RenderGraphRendererSettings>().cascaded_shadows;
    const GPUCameraInfo& camera_info = blackboard.get<RenderGraphRendererFrameInfo>().camera_info;
    const DrawInfo& draw_info = blackboard.get<DrawInfo>();

    const uint32_t num_shadow_casting_directional_lights =
        static_cast<uint32_t>(m_cascade_light_space_matrices.size()) / shadow_settings.num_cascades;

    std::array<float, CascadedShadowsSettings::MAX_NUM_CASCADES> cascade_splits;
    for (uint32_t cascade_idx = 0; cascade_idx < shadow_settings.num_cascades; ++cascade_idx)
    {
        const float clip_range = camera_info.zfar - camera_info.znear;
        cascade_splits[cascade_idx] =
            (camera_info.znear + shadow_settings.cascade_split_factors[cascade_idx] * clip_range) * -1.0f;
    }

    const RGBufferRef light_space_matrices_ref =
        builder.create_structured_buffer<glm::mat4>(m_cascade_light_space_matrices, "LightSpaceMatricesBuffer");
    const RGBufferRef cascade_splits_ref = builder.create_structured_buffer<float>(
        std::span(cascade_splits.data(), shadow_settings.num_cascades), "CascadeSplitsBuffer");

    const uint32_t width = std::max(shadow_settings.resolution * shadow_settings.num_cascades, 1u);
    const uint32_t height = std::max(shadow_settings.resolution * num_shadow_casting_directional_lights, 1u);

    const RGImageRef shadow_map_texture_ref =
        builder.create_texture({width, height}, ImageFormat::D32_SFLOAT, "ShadowMapTexture");
    const RGTextureRtvRef shadow_map_view_ref = builder.create_texture_rtv(shadow_map_texture_ref);

    CascadedShadowMappingParameters params{};
    params.lightSpaceMatrices = builder.create_buffer_srv(light_space_matrices_ref);
    params.transformInfo = draw_info.transform_info_ref;
    params.transformIndices = draw_info.cascaded_shadows_indices_ref;
    params.framebuffer = RGFramebufferAttachments{
        .width = width,
        .height = height,
        .depth_stencil_attachment = shadow_map_view_ref,
    };

    RGResourceGroupLayout layout0{};
    layout0.add_resource(0, params.lightSpaceMatrices, ShaderType::Vertex);
    layout0.add_resource(1, params.transformInfo, ShaderType::Vertex);
    layout0.add_resource(2, params.transformIndices, ShaderType::Vertex);

    const RGResourceGroupRef resource_group0_ref = builder.create_resource_group(layout0);

    CascadedShadowMappingShaderVS vertex_shader;
    CascadedShadowMappingShaderFS fragment_shader;

    RasterizationState raster{};
    raster.depth_clamp = true;
    raster.cull_mode = RasterizationState::CullMode::Front;

    DepthStencilState depth_stencil{};
    depth_stencil.depth_test = true;
    depth_stencil.depth_write = true;

    builder.add_pass(
        "CascadedShadowMapping",
        params,
        RGPassHint::Raster,
        [=, this](CommandBuffer& command, const RGPassResources& resources) {
            const auto framebuffer = resources.get_framebuffer();
            command.begin_render_pass(framebuffer);
            {
                const auto pipeline = get_graphics_pipeline(
                    vertex_shader, fragment_shader, raster, depth_stencil, ColorBlendState{}, framebuffer);
                command.bind_pipeline(pipeline);

                bind_resource_group(command, resources, resource_group0_ref, 0);

                struct PushConstant
                {
                    uint32_t num_cascades;
                    uint32_t num_lights;
                    uint64_t transform_offset;
                };

                PushConstant push_constant{};
                push_constant.num_cascades = shadow_settings.num_cascades;
                push_constant.num_lights = num_shadow_casting_directional_lights;

                const DrawList& list = m_draw_manager->get_draw_list(draw_info.cascaded_shadows_handle);

                for (size_t block_idx = 0; block_idx < list.num_blocks; ++block_idx)
                {
                    const DrawBlock& block = list.blocks[block_idx];

                    const std::string draw_block_name = std::format("DrawBlock:{}", block.pipeline_hash);
                    command.begin_gpu_marker(draw_block_name);

                    for (size_t elem_idx = 0; elem_idx < block.num_elements; ++elem_idx)
                    {
                        const DrawElement& element = block.elements[elem_idx];

                        push_constant.transform_offset = element.transform_offset;
                        command.push_constant(push_constant);

                        const uint32_t num_instances =
                            shadow_settings.num_cascades * push_constant.num_lights * element.instance_count;
                        draw_mesh_instanced(command, *element.mesh, num_instances);
                    }

                    command.end_gpu_marker();
                }
            }
            command.end_render_pass();
        });

    ShadowsInfo& shadows_info = blackboard.add<ShadowsInfo>();
    shadows_info.shadow_map_view_ref = builder.create_texture_srv(shadow_map_texture_ref);
    shadows_info.cascade_splits_ref = builder.create_buffer_srv(cascade_splits_ref);
    shadows_info.light_space_matrices_ref = params.lightSpaceMatrices;
}

void RenderGraphRenderer::add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    const DrawInfo& draw_info = blackboard.get<DrawInfo>();
    const LightsInfo& lights_info = blackboard.get<LightsInfo>();
    const DepthNormalsPrepassInfo& depth_normals_info = blackboard.get<DepthNormalsPrepassInfo>();
    const LightCullingInfo& culling_info = blackboard.get<LightCullingInfo>();
    const ShadowsInfo& shadows_info = blackboard.get<ShadowsInfo>();

    LightingShaderParameters params{};
    params.cameraInfo = frame_info.camera_info_ref;
    params.transformInfo = draw_info.transform_info_ref;
    params.transformIndices = draw_info.main_view_indices_ref;
    params.pointLights = lights_info.point_lights_ref;
    params.directionalLights = lights_info.directional_lights_ref;
    params.visiblePointLightIndices = culling_info.visible_point_light_indices_ref;
    params.lightCullingInfo = culling_info.light_culling_info_ref;
    params.directionalShadowMap = shadows_info.shadow_map_view_ref;
    params.directionalShadowMapSampler = get_sampler_state(
        SamplerStateDescription{
            .address_mode_u = ImageAddressMode::ClampToEdge,
            .address_mode_v = ImageAddressMode::ClampToEdge,
            .address_mode_w = ImageAddressMode::ClampToEdge,
            .border_color = BorderColor::FloatOpaqueWhite,
        });
    params.cascadeSplits = shadows_info.cascade_splits_ref;
    params.lightSpaceMatrices = shadows_info.light_space_matrices_ref;
    params.framebuffer = RGFramebufferAttachments{
        .width = frame_info.width,
        .height = frame_info.height,
        .color_attachments = {frame_info.output_view_ref},
        .depth_stencil_attachment = depth_normals_info.depth_view_rtv_ref,
    };

    GraphicsPipelineDescription pipeline_desc{};
    pipeline_desc.depth_stencil.depth_test = true;
    pipeline_desc.depth_stencil.depth_write = false;
    pipeline_desc.depth_stencil.depth_compare_op = DepthStencilState::DepthCompareOp::LessEqual;

    const RGResourceGroupLayout rg0_layout =
        RGResourceGroupLayout()
            .add_resource(0, frame_info.camera_info_ref, ShaderType::Vertex | ShaderType::Fragment)
            .add_resource(1, draw_info.transform_info_ref, ShaderType::Vertex)
            .add_resource(2, draw_info.main_view_indices_ref, ShaderType::Vertex);
    const RGResourceGroupRef resource_group_ref_0 = builder.create_resource_group(rg0_layout);

    const RGResourceGroupLayout rg1_layout =
        RGResourceGroupLayout()
            .add_resource(0, lights_info.point_lights_ref, ShaderType::Fragment)
            .add_resource(1, lights_info.directional_lights_ref, ShaderType::Fragment)
            .add_resource(2, culling_info.visible_point_light_indices_ref, ShaderType::Fragment)
            .add_resource(3, culling_info.light_culling_info_ref, ShaderType::Fragment)
            .add_resource(4, shadows_info.shadow_map_view_ref, ShaderType::Fragment)
            .add_resource(5, params.directionalShadowMapSampler, ShaderType::Fragment)
            .add_resource(6, shadows_info.cascade_splits_ref, ShaderType::Fragment)
            .add_resource(7, shadows_info.light_space_matrices_ref, ShaderType::Fragment);
    const RGResourceGroupRef resource_group_ref_1 = builder.create_resource_group(rg1_layout);

    builder.add_pass(
        "Lighting", params, RGPassHint::Raster, [=, this](CommandBuffer& command, const RGPassResources& resources) {
            struct PushConstant
            {
                uint64_t transform_offset;
            };

            const auto resource_group_0 = resources.get_resource_group(resource_group_ref_0);
            const auto resource_group_1 = resources.get_resource_group(resource_group_ref_1);

            const auto framebuffer = resources.get_framebuffer();
            command.begin_render_pass(framebuffer);
            {
                const DrawList& list = m_draw_manager->get_draw_list(draw_info.main_view_handle);

                GraphicsPipelineDescription local_pipeline_desc = pipeline_desc;

                for (size_t i = 0; i < list.num_blocks; ++i)
                {
                    const DrawBlock& block = list.blocks[i];

                    const std::string draw_block_name = std::format("DrawBlock:{}", block.pipeline_hash);
                    command.begin_gpu_marker(draw_block_name);

                    const auto pipeline = get_graphics_pipeline(
                        block.vertex_instance,
                        block.fragment_instance,
                        local_pipeline_desc.rasterization,
                        local_pipeline_desc.depth_stencil,
                        local_pipeline_desc.color_blend,
                        framebuffer);
                    command.bind_pipeline(pipeline);

                    command.bind_resource_group(resource_group_0, 0);
                    command.bind_resource_group(resource_group_1, 1);

                    size_t last_material_hash = 0;
                    for (size_t elem_idx = 0; elem_idx < block.num_elements; ++elem_idx)
                    {
                        const DrawElement& element = block.elements[elem_idx];
                        if (last_material_hash != element.material->get_material_hash())
                        {
                            last_material_hash = element.material->get_material_hash();
                            set_material(command, *element.material);
                        }

                        PushConstant push_constant{};
                        push_constant.transform_offset = element.transform_offset;
                        command.push_constant(push_constant);

                        draw_mesh_instanced(command, *element.mesh, static_cast<uint32_t>(element.instance_count));
                    }

                    command.end_gpu_marker();
                }
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

    LightCullingDebugParameters params{};
    params.visiblePointLightIndices = culling_info.visible_point_light_indices_ref;
    params.lightCullingInfo = culling_info.light_culling_info_ref;
    params.framebuffer = RGFramebufferAttachments{
        .width = frame_info.width,
        .height = frame_info.height,
        .color_attachments = {frame_info.output_view_ref},
    };

    RGResourceGroupLayout layout0{};
    layout0.add_resource(0, params.visiblePointLightIndices, ShaderType::Fragment);
    layout0.add_resource(1, params.lightCullingInfo, ShaderType::Fragment);

    const RGResourceGroupRef resource_group0_ref = builder.create_resource_group(layout0);

    LightCullingDebugShaderVS vertex_shader;
    LightCullingDebugShaderFS fragment_shader;

    GraphicsPipelineDescription pipeline_desc{};
    pipeline_desc.depth_stencil.depth_test = false;
    pipeline_desc.depth_stencil.depth_write = false;
    pipeline_desc.color_blend = ColorBlendState{
        .method = ColorBlendState::Method::PerAttachment,
        .attachments =
            {
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
            },
    };

    builder.add_pass(
        "LightCullingDebug",
        params,
        RGPassHint::Raster,
        [=, this](CommandBuffer& command, const RGPassResources& resources) {
            const auto framebuffer = resources.get_framebuffer();
            command.begin_render_pass(framebuffer);
            {
                const auto pipeline = get_graphics_pipeline(
                    vertex_shader,
                    fragment_shader,
                    pipeline_desc.rasterization,
                    pipeline_desc.depth_stencil,
                    pipeline_desc.color_blend,
                    framebuffer);
                command.bind_pipeline(pipeline);

                bind_resource_group(command, resources, resource_group0_ref, 0);

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

    MIZU_RG_SCOPED_GPU_MARKER(builder, "CascadedShadowMappingDebug");

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();
    const DepthNormalsPrepassInfo& depth_normals_info = blackboard.get<DepthNormalsPrepassInfo>();
    const ShadowsInfo& shadows_info = blackboard.get<ShadowsInfo>();

    CascadedShadowMappingDebugShaderVS vertex_shader;
    CascadedShadowMappingDebugCascadesShaderFS cascades_fragment_shader;

    GraphicsPipelineDescription cascades_pipeline_desc{};
    cascades_pipeline_desc.depth_stencil.depth_test = false;
    cascades_pipeline_desc.depth_stencil.depth_write = false;
    cascades_pipeline_desc.color_blend = ColorBlendState{
        .method = ColorBlendState::Method::PerAttachment,
        .attachments =
            {
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
            },
    };

    CascadedShadowMappingDebugCascadesParameters cascades_params{};
    cascades_params.cameraInfo = frame_info.camera_info_ref;
    cascades_params.cascadeSplits = shadows_info.cascade_splits_ref;
    cascades_params.depthTexture = depth_normals_info.depth_view_srv_ref;
    cascades_params.sampler = get_sampler_state({});
    cascades_params.framebuffer = RGFramebufferAttachments{
        .width = frame_info.width,
        .height = frame_info.height,
        .color_attachments = {frame_info.output_view_ref},
    };

    RGResourceGroupLayout cascades_layout0{};
    cascades_layout0.add_resource(0, cascades_params.cameraInfo, ShaderType::Fragment);

    RGResourceGroupLayout cascades_layout1{};
    cascades_layout1.add_resource(0, cascades_params.cascadeSplits, ShaderType::Fragment);
    cascades_layout1.add_resource(1, cascades_params.depthTexture, ShaderType::Fragment);
    cascades_layout1.add_resource(2, cascades_params.sampler, ShaderType::Fragment);

    const RGResourceGroupRef cascades_resource_group0_ref = builder.create_resource_group(cascades_layout0);
    const RGResourceGroupRef cascades_resource_group1_ref = builder.create_resource_group(cascades_layout1);

    builder.add_pass(
        "DrawCascades",
        cascades_params,
        RGPassHint::Raster,
        [=, this](CommandBuffer& command, const RGPassResources& resources) {
            const auto framebuffer = resources.get_framebuffer();
            command.begin_render_pass(framebuffer);
            {
                const auto pipeline = get_graphics_pipeline(
                    vertex_shader,
                    cascades_fragment_shader,
                    cascades_pipeline_desc.rasterization,
                    cascades_pipeline_desc.depth_stencil,
                    cascades_pipeline_desc.color_blend,
                    framebuffer);
                command.bind_pipeline(pipeline);

                bind_resource_group(command, resources, cascades_resource_group0_ref, 0);
                bind_resource_group(command, resources, cascades_resource_group1_ref, 1);

                command.draw(*m_fullscreen_triangle);
            }
            command.end_render_pass();
        });

    const float shadow_map_width = glm::round(static_cast<float>(frame_info.width) * 0.5f);
    const float shadow_map_height = glm::round(static_cast<float>(frame_info.height) * 0.3f);

    CascadedShadowMappingDebugTextureShaderFS texture_fragment_shader;

    GraphicsPipelineDescription texture_pipeline_desc{};

    CascadedShadowMappingDebugTextureParameters texture_params{};
    texture_params.shadowMapTexture = shadows_info.shadow_map_view_ref;
    texture_params.sampler = get_sampler_state({});
    texture_params.framebuffer = RGFramebufferAttachments{
        .width = static_cast<uint32_t>(shadow_map_width),
        .height = static_cast<uint32_t>(shadow_map_height),
        .color_attachments = {frame_info.output_view_ref},
    };

    RGResourceGroupLayout texture_layout1{};
    texture_layout1.add_resource(2, texture_params.sampler, ShaderType::Fragment);
    texture_layout1.add_resource(3, texture_params.shadowMapTexture, ShaderType::Fragment);

    const RGResourceGroupRef texture_resource_group1_ref = builder.create_resource_group(texture_layout1);

    builder.add_pass(
        "DrawShadowMap",
        texture_params,
        RGPassHint::Raster,
        [=, this](CommandBuffer& command, const RGPassResources& resources) {
            const auto framebuffer = resources.get_framebuffer();
            command.begin_render_pass(framebuffer);
            {
                const auto pipeline = get_graphics_pipeline(
                    vertex_shader,
                    texture_fragment_shader,
                    texture_pipeline_desc.rasterization,
                    texture_pipeline_desc.depth_stencil,
                    texture_pipeline_desc.color_blend,
                    framebuffer);
                command.bind_pipeline(pipeline);

                bind_resource_group(command, resources, texture_resource_group1_ref, 1);

                command.draw(*m_fullscreen_triangle);
            }
            command.end_render_pass();
        });
}

void RenderGraphRenderer::get_light_information(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    MIZU_PROFILE_SCOPED;

    const CascadedShadowsSettings& shadow_settings = blackboard.get<RenderGraphRendererSettings>().cascaded_shadows;
    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();

    m_point_lights.clear();
    m_directional_lights.clear();
    m_cascade_light_space_matrices.clear();

    const LightStateManager::IteratorWrapper& wrapper = g_light_state_manager->rend_iterator();

    m_point_lights.reserve(wrapper.size());
    m_directional_lights.reserve(wrapper.size());
    m_cascade_light_space_matrices.reserve(wrapper.size() * shadow_settings.num_cascades);

    for (const LightHandle& handle : wrapper)
    {
        const LightDynamicState& dynamic_state = g_light_state_manager->get_dynamic_state(handle);

        if (handle->is_point_light())
        {
            GPUPointLight light{};
            light.position = handle->get_position();
            light.color = dynamic_state.color;
            light.intensity = dynamic_state.intensity;
            light.cast_shadows = dynamic_state.cast_shadows;
            light.radius = std::get<LightDynamicState::Point>(dynamic_state.data).radius;

            m_point_lights.push_back(light);
        }
        else if (handle->is_directional_light())
        {
            GPUDirectionalLight light{};
            light.position = handle->get_position();
            light.color = dynamic_state.color;
            light.intensity = dynamic_state.intensity;
            light.cast_shadows = dynamic_state.cast_shadows;
            light.direction = std::get<LightDynamicState::Directional>(dynamic_state.data).direction;

            m_directional_lights.push_back(light);
        }
        else
        {
            MIZU_UNREACHABLE("Invalid light type in LightStateManager");
        }
    }

    for (const GPUDirectionalLight& light : m_directional_lights)
    {
        if (!light.cast_shadows)
            continue;

        for (uint32_t cascade_idx = 0; cascade_idx < shadow_settings.num_cascades; ++cascade_idx)
        {
            const float split_dist = shadow_settings.cascade_split_factors[cascade_idx];
            const float last_split_dist =
                cascade_idx == 0 ? 0.0f : shadow_settings.cascade_split_factors[cascade_idx - 1];

            // clang-format off
            glm::vec3 frustum_corners[8] = {
                glm::vec3(-1.0f,  1.0f, 0.0f),
                glm::vec3( 1.0f,  1.0f, 0.0f),
                glm::vec3( 1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f, -1.0f, 0.0f),
                glm::vec3(-1.0f,  1.0f, 1.0f),
                glm::vec3( 1.0f,  1.0f, 1.0f),
                glm::vec3( 1.0f, -1.0f, 1.0f),
                glm::vec3(-1.0f, -1.0f, 1.0f),
            };
            // clang-format on

            for (uint32_t i = 0; i < 8; ++i)
            {
                const glm::vec4 inverted_corner =
                    frame_info.camera_info.inverseViewProj * glm::vec4(frustum_corners[i], 1.0f);
                frustum_corners[i] = inverted_corner / inverted_corner.w;
            }

            for (uint32_t i = 0; i < 4; ++i)
            {
                const glm::vec3 dist = frustum_corners[i + 4] - frustum_corners[i];
                frustum_corners[i + 4] = frustum_corners[i] + (dist * split_dist);
                frustum_corners[i] = frustum_corners[i] + (dist * last_split_dist);
            }

            glm::vec3 frustum_center{0.0f};
            for (uint32_t i = 0; i < 8; ++i)
            {
                frustum_center += frustum_corners[i];
            }
            frustum_center /= 8.0f;

            float radius = 0.0f;
            for (uint32_t i = 0; i < 8; ++i)
            {
                const float distance = glm::length(frustum_corners[i] - frustum_center);
                radius = glm::max(radius, distance);
            }
            radius = glm::ceil(radius * 16.0f) / 16.0f;

            const glm::vec3 max_extents = glm::vec3(radius);
            const glm::vec3 min_extents = -max_extents;

            const glm::vec3 cascade_extents = max_extents - min_extents;
            const glm::vec3 camera_pos = frustum_center - light.direction * -min_extents.z;

            const glm::mat4 light_view = glm::lookAt(camera_pos, frustum_center, glm::vec3(0.0f, 1.0f, 0.0f));
            const glm::mat4 light_proj =
                glm::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.0f, cascade_extents.z);

            const glm::mat4 light_view_proj = light_proj * light_view;
            m_cascade_light_space_matrices.push_back(light_view_proj);
        }
    }

    const RGBufferRef point_lights_ref =
        builder.create_structured_buffer<GPUPointLight>(m_point_lights, "PointLightsBuffer");
    const RGBufferRef directional_lights_ref =
        builder.create_structured_buffer<GPUDirectionalLight>(m_directional_lights, "DirectionalLightsBuffer");

    LightsInfo& lights_info = blackboard.add<LightsInfo>();
    lights_info.point_lights_ref = builder.create_buffer_srv(point_lights_ref);
    lights_info.directional_lights_ref = builder.create_buffer_srv(directional_lights_ref);
}

void RenderGraphRenderer::create_draw_lists(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    MIZU_PROFILE_SCOPED;

    const RenderGraphRendererFrameInfo& frame_info = blackboard.get<RenderGraphRendererFrameInfo>();

    const Job transform_info_job = Job::create([this]() {
        MIZU_PROFILE_SCOPED_NAME("generate_transform_info_job");

        const StaticMeshStateManager::IteratorWrapper& wrapper = g_static_mesh_state_manager->rend_iterator();
        for (uint32_t i = 0; i < wrapper.size(); ++i)
        {
            const StaticMeshHandle& handle = *(wrapper.begin() + i);
            const StaticMeshStaticState& ss = g_static_mesh_state_manager->rend_get_static_state(handle);

            const glm::vec3 translation = ss.transform_handle->get_translation();
            const glm::vec3 rotation = ss.transform_handle->get_rotation();
            const glm::vec3 scale = ss.transform_handle->get_scale();

            glm::mat4 transform{1.0f};
            transform = glm::translate(transform, translation);
            transform = glm::rotate(transform, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            transform = glm::rotate(transform, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            transform = glm::rotate(transform, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
            transform = glm::scale(transform, scale);

            m_transform_info_buffer[i] = InstanceTransformInfo{
                .transform = transform,
                .normal_matrix = glm::transpose(glm::inverse(transform)),
            };
        }
    });

    DrawListHandle main_handle, cascaded_shadows_handle;

    const Job main_view_job = Job::create(
        [this, frame_info](DrawListHandle& output) {
            const Frustum frustum =
                Frustum::from_view_projection(frame_info.camera_info.viewProj, frame_info.camera_info.pos);
            output =
                m_draw_manager->create_draw_list(DrawListType::Opaque, frustum, m_main_view_transform_indices_buffer);
        },
        std::ref(main_handle));

    const Job cascaded_shadows_job = Job::create(
        [this](DrawListHandle& output) {
            // TODO: Using no frustum to include all elements into the cascaded shadows
            output = m_draw_manager->create_draw_list(
                DrawListType::Shadow, Frustum{}, m_cascaded_shadows_transform_indices_buffer);
        },
        std::ref(cascaded_shadows_handle));

    Job jobs[] = {transform_info_job, main_view_job, cascaded_shadows_job};
    const JobSystemHandle handle = g_job_system->schedule(jobs);

    handle.wait();

    const RGBufferRef transform_indices_ref = builder.create_structured_buffer<InstanceTransformInfo>(
        m_transform_info_buffer, "MainViewTransformIndicesBuffer");
    const RGBufferRef main_view_indices_ref = builder.create_structured_buffer<uint64_t>(
        m_main_view_transform_indices_buffer, "MainViewTransformIndicesBuffer");
    const RGBufferRef cascaded_shadows_indices_ref =
        builder.create_structured_buffer<uint64_t>(m_cascaded_shadows_transform_indices_buffer);

    DrawInfo& draw_info = blackboard.add<DrawInfo>();
    draw_info.main_view_handle = main_handle;
    draw_info.cascaded_shadows_handle = cascaded_shadows_handle;
    draw_info.transform_info_ref = builder.create_buffer_srv(transform_indices_ref);
    draw_info.main_view_indices_ref = builder.create_buffer_srv(main_view_indices_ref);
    draw_info.cascaded_shadows_indices_ref = builder.create_buffer_srv(cascaded_shadows_indices_ref);
}

} // namespace Mizu
