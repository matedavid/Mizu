#include "render_graph_renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "base/debug/profiling.h"

#include "renderer/camera.h"
#include "renderer/material/material.h"
#include "renderer/model/mesh.h"
#include "renderer/render_graph_renderer_shaders.h"

#include "render_core/render_graph/render_graph_blackboard.h"
#include "render_core/render_graph/render_graph_builder.h"
#include "render_core/render_graph/render_graph_utils.h"
#include "render_core/resources/buffers.h"
#include "render_core/resources/texture.h"
#include "render_core/rhi/sampler_state.h"

#include "state_manager/light_state_manager.h"
#include "state_manager/static_mesh_state_manager.h"
#include "state_manager/transform_state_manager.h"

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
    float znear;
    float zfar;
};

struct GPULightCullingInfo
{
    glm::uvec2 num_tiles;
};

struct GPUPushConstant
{
    glm::mat4 model;
    glm::mat4 normal_matrix;
};

struct FrameInfo
{
    uint32_t width, height;
    GPUCameraInfo camera_info;

    RGUniformBufferRef camera_info_ref;
    RGStorageBufferRef point_lights_ref;
    RGStorageBufferRef directional_lights_ref;

    RGImageViewRef output_view_ref;
};

struct DepthPrePassInfo
{
    RGImageViewRef depth_view_ref;
    RGImageViewRef normals_view_ref;
};

struct LightCullingInfo
{
    RGStorageBufferRef visible_point_light_indices_ref;
    RGUniformBufferRef light_culling_info_ref;
};

struct ShadowsInfo
{
    RGImageViewRef shadow_map_view_ref;
    RGStorageBufferRef cascade_splits_ref;
    RGStorageBufferRef light_space_matrices_ref;
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
    m_fullscreen_triangle = VertexBuffer::create(vertices, Renderer::get_allocator());
}

void RenderGraphRenderer::build(RenderGraphBuilder& builder, const Camera& camera, const Texture2D& output)
{
    MIZU_PROFILE_SCOPED;

    RenderGraphBlackboard blackboard;

    const uint32_t width = output.get_resource()->get_width();
    const uint32_t height = output.get_resource()->get_height();

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

    const RGUniformBufferRef camera_info_ref = builder.create_uniform_buffer(gpu_camera_info, "CameraInfo");

    const RGTextureRef output_texture_ref =
        builder.register_external_texture(output, {ImageResourceState::Undefined, ImageResourceState::Present});
    const RGImageViewRef output_view_ref = builder.create_image_view(output_texture_ref);

    get_render_meshes(camera);
    get_light_information();

    const RGStorageBufferRef point_lights_ref = builder.create_storage_buffer(m_point_lights, "PointLightsBuffer");
    const RGStorageBufferRef directional_lights_ref =
        builder.create_storage_buffer(m_directional_lights, "DirectionalLightsBuffer");

    FrameInfo& frame_info = blackboard.add<FrameInfo>();
    frame_info.width = width;
    frame_info.height = height;
    frame_info.camera_info = gpu_camera_info;
    frame_info.camera_info_ref = camera_info_ref;
    frame_info.point_lights_ref = point_lights_ref;
    frame_info.directional_lights_ref = directional_lights_ref;
    frame_info.output_view_ref = output_view_ref;

    render_scene(builder, blackboard);
}

void RenderGraphRenderer::render_scene(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    add_depth_pre_pass(builder, blackboard);
    add_light_culling_pass(builder, blackboard);
    add_cascaded_shadow_mapping_pass(builder, blackboard);
    add_lighting_pass(builder, blackboard);

#if 0
    add_light_culling_debug_pass(builder, blackboard);
#endif

#if 0
    add_cascaded_shadow_mapping_debug_pass(builder, blackboard);
#endif
}

void RenderGraphRenderer::add_depth_pre_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const FrameInfo& frame_info = blackboard.get<FrameInfo>();

    const RGTextureRef normals_texture_ref = builder.create_texture<Texture2D>(
        {frame_info.width, frame_info.height}, ImageFormat::RGBA32_SFLOAT, "NormalsTexture");
    const RGImageViewRef normals_view_ref = builder.create_image_view(normals_texture_ref);

    const RGTextureRef depth_texture_ref = builder.create_texture<Texture2D>(
        {frame_info.width, frame_info.height}, ImageFormat::D32_SFLOAT, "DepthTexture");
    const RGImageViewRef depth_view_ref = builder.create_image_view(depth_texture_ref);

    DepthPrePassShader::Parameters params{};
    params.cameraInfo = frame_info.camera_info_ref;
    params.framebuffer = RGFramebufferAttachments{
        .width = frame_info.width,
        .height = frame_info.height,
        .color_attachments = {normals_view_ref},
        .depth_stencil_attachment = depth_view_ref,
    };

    GraphicsPipeline::Description pipeline_desc{};
    pipeline_desc.depth_stencil.depth_test = true;
    pipeline_desc.depth_stencil.depth_write = true;

    add_raster_pass(
        builder,
        "DepthPrePass",
        DepthPrePassShader{},
        params,
        pipeline_desc,
        [this](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
            for (const RenderMesh& render_mesh : m_render_meshes)
            {
                GPUPushConstant push_constant{};
                push_constant.model = render_mesh.transform;
                push_constant.normal_matrix = glm::transpose(glm::inverse(render_mesh.transform));
                command.push_constant("pushConstant", push_constant);

                RHIHelpers::draw_mesh(command, *render_mesh.mesh);
            }
        });

    DepthPrePassInfo& depth_pre_pass_info = blackboard.add<DepthPrePassInfo>();
    depth_pre_pass_info.depth_view_ref = depth_view_ref;
    depth_pre_pass_info.normals_view_ref = normals_view_ref;
}

void RenderGraphRenderer::add_light_culling_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const DepthPrePassInfo& depth_info = blackboard.get<DepthPrePassInfo>();

    // Should match values defined in LightCullingCommon.slang
    constexpr uint32_t TILE_SIZE = 16;
    constexpr uint32_t MAX_LIGHTS_PER_TILE = 128;

    const glm::uvec3 group_count =
        RHIHelpers::compute_group_count({frame_info.width, frame_info.height, 1.0f}, {TILE_SIZE, TILE_SIZE, 1.0f});

    const uint32_t num_tiles = group_count.x * group_count.y;
    const uint32_t point_lights_size = num_tiles * MAX_LIGHTS_PER_TILE * sizeof(uint32_t);

    const RGStorageBufferRef visible_point_light_indices_ref =
        builder.create_storage_buffer(point_lights_size, "VisiblePointLightIndices");

    GPULightCullingInfo gpu_light_culling_info{};
    gpu_light_culling_info.num_tiles = glm::uvec2(group_count);

    const RGUniformBufferRef light_culling_info_ref =
        builder.create_uniform_buffer(gpu_light_culling_info, "LightCullingInfo");

    LightCullingShader::Parameters params{};
    params.cameraInfo = frame_info.camera_info_ref;
    params.pointLights = frame_info.point_lights_ref;
    params.depthTextureSampler = RHIHelpers::get_sampler_state(SamplingOptions{});
    params.visiblePointLightIndices = visible_point_light_indices_ref;
    params.lightCullingInfo = light_culling_info_ref;
    params.depthTexture = depth_info.depth_view_ref;

    MIZU_RG_ADD_COMPUTE_PASS(builder, "LightCulling", LightCullingShader{}, params, group_count);

    LightCullingInfo& culling_info = blackboard.add<LightCullingInfo>();
    culling_info.visible_point_light_indices_ref = visible_point_light_indices_ref;
    culling_info.light_culling_info_ref = light_culling_info_ref;
}

void RenderGraphRenderer::add_cascaded_shadow_mapping_pass(
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    constexpr uint32_t SHADOW_MAP_RESOLUTION = 2048;
    constexpr uint32_t NUM_CASCADES = 4;

    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const GPUCameraInfo& camera = frame_info.camera_info;

    const std::array<float, NUM_CASCADES> cascade_splits_factor = {0.05f, 0.15f, 0.50f, 1.0f};

    std::vector<glm::mat4> light_space_matrices;
    light_space_matrices.reserve(m_directional_lights.size() * NUM_CASCADES);

    std::vector<float> cascade_splits;
    cascade_splits.resize(NUM_CASCADES);

    for (const DirectionalLight& light : m_directional_lights)
    {
        if (!light.cast_shadows)
            continue;

        for (uint32_t cascade_idx = 0; cascade_idx < NUM_CASCADES; ++cascade_idx)
        {
            const float split_dist = cascade_splits_factor[cascade_idx];
            const float last_split_dist = cascade_idx == 0 ? 0.0f : cascade_splits_factor[cascade_idx - 1];

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
                const glm::vec4 inverted_corner = camera.inverseViewProj * glm::vec4(frustum_corners[i], 1.0f);
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
            light_space_matrices.push_back(light_view_proj);

            const float clip_range = camera.zfar - camera.znear;
            cascade_splits[cascade_idx] = (camera.znear + split_dist * clip_range) * -1.0f;
        }
    }

    const uint32_t num_shadow_casting_directional_lights =
        static_cast<uint32_t>(light_space_matrices.size()) / NUM_CASCADES;

    const RGStorageBufferRef light_space_matrices_ref =
        builder.create_storage_buffer(light_space_matrices, "LightSpaceMatricesBuffer");
    const RGStorageBufferRef cascade_splits_ref = builder.create_storage_buffer(cascade_splits, "CascadeSplitsBuffer");

    const uint32_t width = SHADOW_MAP_RESOLUTION * NUM_CASCADES;
    const uint32_t height = SHADOW_MAP_RESOLUTION * num_shadow_casting_directional_lights;

    const RGTextureRef shadow_map_texture_ref =
        builder.create_texture<Texture2D>({width, height}, ImageFormat::D32_SFLOAT, "ShadowMapTexture");
    const RGImageViewRef shadow_map_view_ref = builder.create_image_view(shadow_map_texture_ref);

    CascadedShadowMappingShader::Parameters params{};
    params.lightSpaceMatrices = light_space_matrices_ref;
    params.framebuffer = RGFramebufferAttachments{
        .width = width,
        .height = height,
        .depth_stencil_attachment = shadow_map_view_ref,
    };

    GraphicsPipeline::Description pipeline_desc{};
    pipeline_desc.rasterization = RasterizationState{
        .depth_clamp = true,
        .cull_mode = RasterizationState::CullMode::Front,
    };
    pipeline_desc.depth_stencil = DepthStencilState{
        .depth_test = true,
        .depth_write = true,
    };

    add_raster_pass(
        builder,
        "CascadedShadowMapping",
        CascadedShadowMappingShader{},
        params,
        pipeline_desc,
        [=, this](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
            struct PushConstant
            {
                glm::mat4 model;
                uint32_t num_cascades;
                uint32_t num_lights;
            };

            PushConstant push_constant{};
            push_constant.num_cascades = NUM_CASCADES;
            push_constant.num_lights = num_shadow_casting_directional_lights;

            for (const RenderMesh& mesh : m_render_meshes)
            {
                push_constant.model = mesh.transform;

                const uint32_t num_instances = NUM_CASCADES * num_shadow_casting_directional_lights;

                command.push_constant("pushConstant", push_constant);
                RHIHelpers::draw_mesh_instanced(command, *mesh.mesh, num_instances);
            }
        });

    ShadowsInfo& shadows_info = blackboard.add<ShadowsInfo>();
    shadows_info.shadow_map_view_ref = shadow_map_view_ref;
    shadows_info.cascade_splits_ref = cascade_splits_ref;
    shadows_info.light_space_matrices_ref = light_space_matrices_ref;
}

void RenderGraphRenderer::add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const DepthPrePassInfo& depth_info = blackboard.get<DepthPrePassInfo>();
    const LightCullingInfo& culling_info = blackboard.get<LightCullingInfo>();
    const ShadowsInfo& shadows_info = blackboard.get<ShadowsInfo>();

    LightingShaderParameters params{};
    params.cameraInfo = frame_info.camera_info_ref;
    params.pointLights = frame_info.point_lights_ref;
    params.directionalLights = frame_info.directional_lights_ref;
    params.visiblePointLightIndices = culling_info.visible_point_light_indices_ref;
    params.lightCullingInfo = culling_info.light_culling_info_ref;
    params.directionalShadowMap = shadows_info.shadow_map_view_ref;
    params.directionalShadowMapSampler = RHIHelpers::get_sampler_state(SamplingOptions{
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
        .depth_stencil_attachment = depth_info.depth_view_ref,
    };

    GraphicsPipeline::Description pipeline_desc{};
    pipeline_desc.depth_stencil.depth_test = true;
    pipeline_desc.depth_stencil.depth_write = false;
    pipeline_desc.depth_stencil.depth_compare_op = DepthStencilState::DepthCompareOp::LessEqual;

    const auto rg0_layout = RGResourceGroupLayout().add_resource(
        0, frame_info.camera_info_ref, ShaderType::Vertex | ShaderType::Fragment, ShaderBufferProperty::Type::Uniform);
    const RGResourceGroupRef resource_group_ref_0 = builder.create_resource_group(rg0_layout);

    const auto rg1_layout =
        RGResourceGroupLayout()
            .add_resource(0, frame_info.point_lights_ref, ShaderType::Fragment, ShaderBufferProperty::Type::Storage)
            .add_resource(
                1, frame_info.directional_lights_ref, ShaderType::Fragment, ShaderBufferProperty::Type::Storage)
            .add_resource(
                2,
                culling_info.visible_point_light_indices_ref,
                ShaderType::Fragment,
                ShaderBufferProperty::Type::Storage)
            .add_resource(
                3, culling_info.light_culling_info_ref, ShaderType::Fragment, ShaderBufferProperty::Type::Uniform)
            .add_resource(4, shadows_info.shadow_map_view_ref, ShaderType::Fragment, ShaderImageProperty::Type::Sampled)
            .add_resource(5, params.directionalShadowMapSampler, ShaderType::Fragment)
            .add_resource(6, shadows_info.cascade_splits_ref, ShaderType::Fragment, ShaderBufferProperty::Type::Storage)
            .add_resource(
                7, shadows_info.light_space_matrices_ref, ShaderType::Fragment, ShaderBufferProperty::Type::Storage);

    const RGResourceGroupRef resource_group_ref_1 = builder.create_resource_group(rg1_layout);

    builder.add_pass(
        "Lighting", params, RGPassHint::Raster, [=, this](CommandBuffer& command, const RGPassResources& resources) {
            const auto framebuffer = resources.get_framebuffer();

            auto render_pass = Mizu::RenderPass::create(RenderPass::Description{.target_framebuffer = framebuffer});

            const auto resource_group_0 = resources.get_resource_group(resource_group_ref_0);
            const auto resource_group_1 = resources.get_resource_group(resource_group_ref_1);

            command.begin_render_pass(render_pass);
            {
                uint64_t last_material_hash = 0;
                for (const RenderMesh& render_mesh : m_render_meshes)
                {
                    if (render_mesh.material->get_hash() != last_material_hash)
                    {
                        RHIHelpers::set_material(command, *render_mesh.material, pipeline_desc);
                        command.bind_resource_group(resource_group_0, 0);
                        command.bind_resource_group(resource_group_1, 1);

                        last_material_hash = render_mesh.material->get_hash();
                    }

                    GPUPushConstant push_constant{};
                    push_constant.model = render_mesh.transform;
                    push_constant.normal_matrix = glm::transpose(glm::inverse(render_mesh.transform));
                    command.push_constant("pushConstant", push_constant);

                    RHIHelpers::draw_mesh(command, *render_mesh.mesh);
                }
            }
            command.end_render_pass();
        });
}

void RenderGraphRenderer::add_light_culling_debug_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
    const
{
    MIZU_PROFILE_SCOPED;

    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const LightCullingInfo& culling_info = blackboard.get<LightCullingInfo>();

    LightCullingDebugShader::Parameters params{};
    params.visiblePointLightIndices = culling_info.visible_point_light_indices_ref;
    params.lightCullingInfo = culling_info.light_culling_info_ref;
    params.framebuffer = RGFramebufferAttachments{
        .width = frame_info.width,
        .height = frame_info.height,
        .color_attachments = {frame_info.output_view_ref},
    };

    GraphicsPipeline::Description pipeline_desc{};
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

    add_raster_pass(
        builder,
        "LightCullingDebug",
        LightCullingDebugShader{},
        params,
        pipeline_desc,
        [this](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
            command.draw(*m_fullscreen_triangle);
        });
}

void RenderGraphRenderer::add_cascaded_shadow_mapping_debug_pass(
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard) const
{
    MIZU_PROFILE_SCOPED;

    MIZU_RG_SCOPED_GPU_MARKER(builder, "CascadedShadowMappingDebug");

    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const DepthPrePassInfo& depth_info = blackboard.get<DepthPrePassInfo>();
    const ShadowsInfo& shadows_info = blackboard.get<ShadowsInfo>();

    GraphicsPipeline::Description pipeline_desc{};
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

    CascadedShadowMappingDebugCascadesShader::Parameter cascades_params{};
    cascades_params.cameraInfo = frame_info.camera_info_ref;
    cascades_params.cascadeSplits = shadows_info.cascade_splits_ref;
    cascades_params.depthTexture = depth_info.depth_view_ref;
    cascades_params.sampler = RHIHelpers::get_sampler_state({});
    cascades_params.framebuffer = RGFramebufferAttachments{
        .width = frame_info.width,
        .height = frame_info.height,
        .color_attachments = {frame_info.output_view_ref},
    };

    add_raster_pass(
        builder,
        "DrawCascades",
        CascadedShadowMappingDebugCascadesShader{},
        cascades_params,
        pipeline_desc,
        [this](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
            command.draw(*m_fullscreen_triangle);
        });

    const float shadow_map_width = glm::round(frame_info.width * 0.5f);
    const float shadow_map_height = glm::round(frame_info.height * 0.3f);

    CascadedShadowMappingDebugTextureShader::Parameter texture_params{};
    texture_params.shadowMapTexture = shadows_info.shadow_map_view_ref;
    texture_params.sampler = RHIHelpers::get_sampler_state({});
    texture_params.framebuffer = RGFramebufferAttachments{
        .width = static_cast<uint32_t>(shadow_map_width),
        .height = static_cast<uint32_t>(shadow_map_height),
        .color_attachments = {frame_info.output_view_ref},
    };

    add_raster_pass(
        builder,
        "DrawShadowMap",
        CascadedShadowMappingDebugTextureShader{},
        texture_params,
        GraphicsPipeline::Description{},
        [this](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
            command.draw(*m_fullscreen_triangle);
        });
}

void RenderGraphRenderer::get_render_meshes(const Camera& camera)
{
    MIZU_PROFILE_SCOPED;

    m_render_meshes.clear();

    for (const StaticMeshHandle& handle : g_static_mesh_state_manager->rend_iterator())
    {
        const StaticMeshStaticState& static_state = g_static_mesh_state_manager->get_static_state(handle);

        if (static_state.mesh == nullptr)
        {
            MIZU_LOG_WARNING("StaticMesh with handle: '{}' does not have a valid mesh", handle.get_internal_id());
            continue;
        }

        if (static_state.material == nullptr)
        {
            MIZU_LOG_WARNING("StaticMesh with handle: '{}' does not have a valid material", handle.get_internal_id());
            continue;
        }

        const glm::vec3 rotation = static_state.transform_handle->get_rotation();

        glm::mat4 transform{1.0f};
        transform = glm::translate(transform, static_state.transform_handle->get_translation());
        transform = glm::rotate(transform, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, static_state.transform_handle->get_scale());

        const BBox& aabb = static_state.mesh->bbox();
        const BBox transformed_aabb = BBox{
            transform * glm::vec4(aabb.min(), 1.0f),
            transform * glm::vec4(aabb.max(), 1.0f),
        };

        if (!camera.is_inside_frustum(transformed_aabb))
        {
            // TODO: ignore because of cascaded shadow mapping
            // continue;
        }

        RenderMesh render_mesh{};
        render_mesh.mesh = static_state.mesh;
        render_mesh.material = static_state.material;
        render_mesh.transform = transform;

        m_render_meshes.push_back(render_mesh);
    }

    // Sort render meshes based on:
    // 1. By material (to prevent changes of pipeline state)
    // TODO: 2. By mesh (to combine entities with same material and mesh, and only bind vertex/index buffer once OR
    // instance the meshes???)
    std::sort(m_render_meshes.begin(), m_render_meshes.end(), [](const RenderMesh& left, const RenderMesh& right) {
        if (left.material->get_hash() != right.material->get_hash())
        {
            return left.material->get_hash() < right.material->get_hash();
        }

        return left.mesh < right.mesh;
    });
}

void RenderGraphRenderer::get_light_information()
{
    MIZU_PROFILE_SCOPED;

    m_point_lights.clear();
    m_directional_lights.clear();

    for (const LightHandle& handle : g_light_state_manager->rend_iterator())
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
}

} // namespace Mizu
