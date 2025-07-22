#include "render_graph_renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/camera.h"
#include "renderer/material/material.h"
#include "renderer/model/mesh.h"
#include "renderer/render_graph_renderer_shaders.h"

#include "render_core/render_graph/render_graph_blackboard.h"
#include "render_core/render_graph/render_graph_builder.h"
#include "render_core/render_graph/render_graph_utils.h"
#include "render_core/resources/texture.h"
#include "render_core/rhi/sampler_state.h"
#include "render_core/shader/shader_declaration.h"

#include "state_manager/light_state_manager.h"
#include "state_manager/static_mesh_state_manager.h"
#include "state_manager/transform_state_manager.h"

namespace Mizu
{

struct FrameInfo
{
    uint32_t width, height;
    RGUniformBufferRef camera_info_ref;
    RGStorageBufferRef point_lights_ref;
    RGStorageBufferRef directional_lights_ref;

    RGImageViewRef output_view_ref;
};

struct DepthPrePassInfo
{
    RGImageViewRef depth_view_ref;
};

struct LightCullingInfo
{
    RGStorageBufferRef visible_point_light_indices_ref;
    RGUniformBufferRef light_culling_info_ref;
};

struct GPUCameraInfo
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 inverseView;
    glm::mat4 inverseProj;
    glm::mat4 inverseViewProj;
    glm::vec3 pos;
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

void RenderGraphRenderer::build(RenderGraphBuilder& builder, const Camera& camera, const Texture2D& output)
{
    RenderGraphBlackboard blackboard;

    const uint32_t width = output.get_resource()->get_width();
    const uint32_t height = output.get_resource()->get_height();

    GPUCameraInfo gpu_camera_info{};
    gpu_camera_info.view = camera.get_view_matrix();
    gpu_camera_info.proj = camera.get_projection_matrix();
    gpu_camera_info.inverseView = glm::inverse(gpu_camera_info.view);
    gpu_camera_info.inverseProj = glm::inverse(gpu_camera_info.proj);
    gpu_camera_info.inverseViewProj = glm::inverse(gpu_camera_info.proj * gpu_camera_info.view);
    gpu_camera_info.pos = camera.get_position();

    const RGUniformBufferRef camera_info_ref = builder.create_uniform_buffer(gpu_camera_info, "CameraInfo");

    const RGTextureRef output_texture_ref =
        builder.register_external_texture(output, {ImageResourceState::Undefined, ImageResourceState::Present});
    const RGImageViewRef output_view_ref = builder.create_image_view(output_texture_ref);

    get_render_meshes();
    get_light_information();

    const RGStorageBufferRef point_lights_ref = builder.create_storage_buffer(m_point_lights, "PointLightsBuffer");
    const RGStorageBufferRef directional_lights_ref =
        builder.create_storage_buffer(m_directional_lights, "DirectionalLightsBuffer");

    FrameInfo& frame_info = blackboard.add<FrameInfo>();
    frame_info.width = width;
    frame_info.height = height;
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
    add_lighting_pass(builder, blackboard);
}

void RenderGraphRenderer::add_depth_pre_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    FrameInfo& frame_info = blackboard.get<FrameInfo>();

    const RGTextureRef depth_texture_ref = builder.create_texture<Texture2D>(
        {frame_info.width, frame_info.height}, ImageFormat::D32_SFLOAT, "DepthTexture");
    const RGImageViewRef depth_view_ref = builder.create_image_view(depth_texture_ref);

    DepthPrePassShader::Parameters params{};
    params.cameraInfo = frame_info.camera_info_ref;
    params.framebuffer = RGFramebufferAttachments{
        .width = frame_info.width,
        .height = frame_info.height,
        .depth_stencil_attachment = depth_view_ref,
    };

    GraphicsPipeline::Description pipeline_desc{};
    pipeline_desc.depth_stencil.depth_test = true;
    pipeline_desc.depth_stencil.depth_write = true;

    add_graphics_pass(
        builder,
        "DepthPrePass",
        DepthPrePassShader{},
        params,
        pipeline_desc,
        [this](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
            for (const RenderMesh& render_mesh : m_render_meshes)
            {
                GPUPushConstant model{};
                model.model = render_mesh.transform;
                model.normal_matrix = glm::transpose(glm::inverse(render_mesh.transform));
                command.push_constant("modelInfo", model);

                RHIHelpers::draw_mesh(command, *render_mesh.mesh);
            }
        });

    DepthPrePassInfo& depth_pre_pass_info = blackboard.add<DepthPrePassInfo>();
    depth_pre_pass_info.depth_view_ref = depth_view_ref;
}

void RenderGraphRenderer::add_light_culling_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
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

    /*
    {
        const RGTextureRef tmp_debug_texture_ref = builder.create_texture<Texture2D>(
            {frame_info.width, frame_info.height}, ImageFormat::RGBA8_UNORM, "LightCullingDebugTexture");
        const RGImageViewRef tmp_debug_image_view_ref = builder.create_image_view(tmp_debug_texture_ref);

        LightCullingDebugShader::Parameters debug_params{};
        debug_params.visiblePointLightIndices = visible_point_light_indices_ref;
        debug_params.output = tmp_debug_image_view_ref;

        MIZU_RG_ADD_COMPUTE_PASS(builder, "LightCullingDebug", LightCullingDebugShader{}, debug_params, group_count);
    }
    */
}

void RenderGraphRenderer::add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const DepthPrePassInfo& depth_info = blackboard.get<DepthPrePassInfo>();
    const LightCullingInfo& culling_info = blackboard.get<LightCullingInfo>();

    LightingShaderParameters params{};
    params.cameraInfo = frame_info.camera_info_ref;
    params.pointLights = frame_info.point_lights_ref;
    params.directionalLights = frame_info.directional_lights_ref;
    params.visiblePointLightIndices = culling_info.visible_point_light_indices_ref;
    params.lightCullingInfo = culling_info.light_culling_info_ref;
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
                3, culling_info.light_culling_info_ref, ShaderType::Fragment, ShaderBufferProperty::Type::Uniform);
    const RGResourceGroupRef resource_group_ref_1 = builder.create_resource_group(rg1_layout);

    builder.add_pass(
        "Lighting", params, RGPassHint::Graphics, [=, this](CommandBuffer& command, const RGPassResources& resources) {
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

                    GPUPushConstant model{};
                    model.model = render_mesh.transform;
                    model.normal_matrix = glm::transpose(glm::inverse(render_mesh.transform));
                    command.push_constant("modelInfo", model);

                    RHIHelpers::draw_mesh(command, *render_mesh.mesh);
                }
            }
            command.end_render_pass();
        });
}

void RenderGraphRenderer::get_render_meshes()
{
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
