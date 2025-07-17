#include "render_graph_renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/camera.h"
#include "renderer/model/mesh.h"
#include "renderer/render_graph_renderer_shaders.h"

#include "render_core/render_graph/render_graph_blackboard.h"
#include "render_core/render_graph/render_graph_builder.h"
#include "render_core/render_graph/render_graph_utils.h"
#include "render_core/resources/texture.h"
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

struct GPUCameraInfo
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 inverseViewProj;
    glm::vec3 pos;
};

struct GPUPushConstant
{
    glm::mat4 model;
};

void RenderGraphRenderer::build(RenderGraphBuilder& builder, const Camera& camera, const Texture2D& output)
{
    RenderGraphBlackboard blackboard;

    const uint32_t width = output.get_resource()->get_width();
    const uint32_t height = output.get_resource()->get_height();

    GPUCameraInfo gpu_camera_info{};
    gpu_camera_info.view = camera.get_view_matrix();
    gpu_camera_info.proj = camera.get_projection_matrix();
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
    add_simple_lighting_pass(builder, blackboard);
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

                command.push_constant("modelInfo", model);
                RHIHelpers::draw_mesh(command, *render_mesh.mesh);
            }
        });

    DepthPrePassInfo& depth_pre_pass_info = blackboard.add<DepthPrePassInfo>();
    depth_pre_pass_info.depth_view_ref = depth_view_ref;
}

void RenderGraphRenderer::add_simple_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const
{
    const FrameInfo& frame_info = blackboard.get<FrameInfo>();
    const DepthPrePassInfo& depth_info = blackboard.get<DepthPrePassInfo>();

    SimpleLightingShader::Parameters params{};
    params.cameraInfo = frame_info.camera_info_ref;
    params.pointLights = frame_info.point_lights_ref;
    params.directionalLights = frame_info.directional_lights_ref;
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

    add_graphics_pass(
        builder,
        "SimpleLighting",
        SimpleLightingShader{},
        params,
        pipeline_desc,
        [&](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
            for (const RenderMesh& render_mesh : m_render_meshes)
            {
                GPUPushConstant model{};
                model.model = render_mesh.transform;

                command.push_constant("modelInfo", model);
                RHIHelpers::draw_mesh(command, *render_mesh.mesh);
            }
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
