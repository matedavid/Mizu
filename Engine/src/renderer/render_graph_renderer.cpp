#include "render_graph_renderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include "renderer/camera.h"

#include "render_core/render_graph/render_graph_builder.h"
#include "render_core/render_graph/render_graph_utils.h"
#include "render_core/resources/texture.h"
#include "render_core/shader/shader_declaration.h"

#include "state_manager/static_mesh_state_manager.h"
#include "state_manager/transform_state_manager.h"

namespace Mizu
{

class SimpleColorShader : public GraphicsShaderDeclaration
{
  public:
    // clang-format off
    BEGIN_SHADER_PARAMETERS(Parameters)
        SHADER_PARAMETER_RG_UNIFORM_BUFFER(cameraInfo)
        SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
    END_SHADER_PARAMETERS()
    // clang-format on

    IMPLEMENT_GRAPHICS_SHADER_DECLARATION(
        "/EngineShaders/SimpleColor.vert.spv",
        "vsMain",
        "/EngineShaders/SimpleColor.frag.spv",
        "fsMain")
};

struct GPUCameraInfo
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 pos;
};

void RenderGraphRenderer::build(RenderGraphBuilder& builder, const Camera& camera, const Texture2D& output)
{
    const uint32_t width = output.get_resource()->get_width();
    const uint32_t height = output.get_resource()->get_height();

    GPUCameraInfo gpu_camera_info{};
    gpu_camera_info.view = camera.get_view_matrix();
    gpu_camera_info.proj = camera.get_projection_matrix();
    gpu_camera_info.pos = camera.get_position();

    const RGUniformBufferRef camera_info_ref = builder.create_uniform_buffer(gpu_camera_info, "CameraInfo");

    const RGTextureRef output_texture_ref =
        builder.register_external_texture(output, {ImageResourceState::Undefined, ImageResourceState::Present});
    const RGImageViewRef output_view_ref = builder.create_image_view(output_texture_ref);

    const RGTextureRef depth_texture_ref =
        builder.create_texture<Texture2D>({width, height}, ImageFormat::D32_SFLOAT, "SimpleColorDepth");
    const RGImageViewRef depth_view_ref = builder.create_image_view(depth_texture_ref);

    SimpleColorShader::Parameters params{};
    params.cameraInfo = camera_info_ref;
    params.framebuffer.width = width;
    params.framebuffer.height = height;
    params.framebuffer.color_attachments = {output_view_ref};
    params.framebuffer.depth_stencil_attachment = depth_view_ref;

    GraphicsPipeline::Description pipeline_desc{};
    pipeline_desc.depth_stencil.depth_test = true;
    pipeline_desc.depth_stencil.depth_write = true;

    add_graphics_pass(
        builder,
        "SimpleColorPass",
        SimpleColorShader{},
        params,
        pipeline_desc,
        [=](CommandBuffer& command, [[maybe_unused]] const RGPassResources& resources) {
            for (const StaticMeshHandle& handle : g_static_mesh_state_manager->rend_iterator())
            {
                const StaticMeshStaticState& static_state = g_static_mesh_state_manager->rend_get_static_state(handle);

                glm::mat4 transform{1.0f};
                transform = glm::translate(transform, static_state.transform_handle->get_translation());

                struct PushConstant
                {
                    glm::mat4 model;
                };

                PushConstant push_constant{};
                push_constant.model = transform;

                command.push_constant("modelInfo", push_constant);
                command.draw_indexed(*static_state.vb, *static_state.ib);
            }
        });
}

} // namespace Mizu
