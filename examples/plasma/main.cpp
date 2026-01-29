#include <Mizu/Mizu.h>

#include <Mizu/Extensions/AssimpLoader.h>
#include <Mizu/Extensions/CameraControllers.h>

using namespace Mizu;

#ifndef MIZU_EXAMPLE_PATH
#define MIZU_EXAMPLE_PATH "./"
#endif

#include "plasma_shaders.h"

constexpr uint32_t WIDTH = 1920;
constexpr uint32_t HEIGHT = 1080;

struct CameraUBO
{
    glm::mat4 view;
    glm::mat4 projection;
};

class PlasmaSimulation : public GameSimulation
{
  public:
    void init() override
    {
        m_camera_controller = FirstPersonCameraController(
            glm::radians(60.0f), static_cast<float>(WIDTH) / static_cast<float>(HEIGHT), 0.001f, 100.0f);
        m_camera_controller.set_position({0.0f, 0.0f, 4.0f});
        m_camera_controller.set_config(
            FirstPersonCameraController::Config{
                .lateral_rotation_sensitivity = 5.0f,
                .vertical_rotation_sensitivity = 5.0f,
                .rotate_modifier_key = MouseButton::Right,
            });
    }

    void update(double dt) override
    {
        m_camera_controller.update(dt);
        sim_set_camera_state(m_camera_controller);
    }

  private:
    FirstPersonCameraController m_camera_controller;
};

class PlasmaRenderModule : public IRenderModule
{
  public:
    PlasmaRenderModule()
    {
        const auto example_path = std::filesystem::path(MIZU_EXAMPLE_PATH);
        const auto mesh_path = example_path / "cube.fbx";

        const auto loader = AssimpLoader::load(mesh_path);
        MIZU_ASSERT(loader.has_value(), "Failed to load: {}", mesh_path.string());
        m_cube_mesh = loader->get_meshes()[0];

        m_camera_ubo = BufferUtils::create_constant_buffer<CameraUBO>(CameraUBO{}, "CameraInfo");

        ShaderManager::get().add_shader_mapping("/PlasmaShaders", MIZU_ENGINE_SHADERS_PATH);
    }

    void build_render_graph(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
    {
        // clang-format off
        BEGIN_SHADER_PARAMETERS(BaseParameters)
            SHADER_PARAMETER_RG_BUFFER_CBV(uCameraInfo)
        END_SHADER_PARAMETERS()
        // clang-format on

        const FrameInfo& frame_info = blackboard.get<FrameInfo>();
        m_time += frame_info.last_frame_time;

        const Camera& camera = rend_get_camera_state();

        const CameraUBO camera_ubo = {
            .view = camera.get_view_matrix(),
            .projection = camera.get_projection_matrix(),
        };
        m_camera_ubo->set_data(reinterpret_cast<const uint8_t*>(&camera_ubo), sizeof(CameraUBO), 0);

        const uint32_t width = frame_info.width;
        const uint32_t height = frame_info.height;

        const RGImageRef plasma_texture_ref =
            builder.create_texture({width, height}, ImageFormat::R8G8B8A8_UNORM, "PlasmaTexture");

        // clang-format off
        BEGIN_SHADER_PARAMETERS(ComputeShaderParameters)
            SHADER_PARAMETER_RG_TEXTURE_UAV(uOutput)
        END_SHADER_PARAMETERS()
        // clang-format on

        ComputeShaderParameters compute_params{};
        compute_params.uOutput = builder.create_texture_uav(plasma_texture_ref);

        ComputeShaderCS compute_shader;

        builder.add_pass(
            "CreatePlasma",
            compute_params,
            RGPassHint::Compute,
            [=, this](CommandBuffer& command, const RGPassResources resources) {
                const auto pipeline = get_compute_pipeline(compute_shader);
                command.bind_pipeline(pipeline);

                std::array descriptor_set_layout = {
                    DescriptorItem::TextureUav(0, 1, ShaderType::Compute),
                };

                std::array descriptor_set_writes = {
                    WriteDescriptor::TextureUav(0, resources.get_texture_uav(compute_params.uOutput)),
                };

                const auto transient_descriptor_set = g_render_device->allocate_descriptor_set(
                    descriptor_set_layout, DescriptorSetAllocationType::Transient);
                transient_descriptor_set->update(descriptor_set_writes);

                command.bind_descriptor_set(transient_descriptor_set, 0);

                struct ComputeShaderConstant
                {
                    uint32_t width;
                    uint32_t height;
                    float time;
                };

                const ComputeShaderConstant constant_info{
                    .width = width,
                    .height = height,
                    .time = static_cast<float>(m_time),
                };

                constexpr uint32_t LOCAL_SIZE = 16;
                const auto group_count =
                    glm::uvec3((width + LOCAL_SIZE - 1) / LOCAL_SIZE, (height + LOCAL_SIZE - 1) / LOCAL_SIZE, 1);

                command.push_constant(constant_info);
                command.dispatch(group_count);
            });

        ImageResourceViewDescription output_view_desc{};
        output_view_desc.override_format = ImageFormat::R8G8B8A8_SRGB;
        const RGTextureRtvRef output_texture_rtv =
            builder.create_texture_rtv(frame_info.output_texture_ref, output_view_desc);

        const RGImageRef depth_texture_ref =
            builder.create_texture({width, height}, ImageFormat::D32_SFLOAT, "DepthTexture");
        const RGTextureRtvRef depth_texture_view_ref = builder.create_texture_rtv(depth_texture_ref);

        const RGBufferRef camera_ubo_ref = builder.register_external_constant_buffer(
            m_camera_ubo,
            {.input_state = BufferResourceState::ShaderReadOnly, .output_state = BufferResourceState::ShaderReadOnly});

        // clang-format off
        BEGIN_SHADER_PARAMETERS_INHERIT(TextureShaderParameters, BaseParameters)
            SHADER_PARAMETER_RG_TEXTURE_SRV(uTexture)
            SHADER_PARAMETER_SAMPLER_STATE(uTexture_Sampler)

            SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
        END_SHADER_PARAMETERS()
        // clang-format on

        TextureShaderParameters texture_pass_params{};
        texture_pass_params.uCameraInfo = builder.create_buffer_cbv(camera_ubo_ref);
        texture_pass_params.uTexture = builder.create_texture_srv(plasma_texture_ref);
        texture_pass_params.uTexture_Sampler = get_sampler_state(SamplerStateDescription{});
        texture_pass_params.framebuffer = RGFramebufferAttachments{
            .width = width,
            .height = height,
            .color_attachments = {output_texture_rtv},
            .depth_stencil_attachment = depth_texture_view_ref,
        };

        TextureShaderVS texture_vertex_shader;
        TextureShaderFS texture_fragment_shader;

        GraphicsPipelineDescription texture_pipeline_desc{};
        texture_pipeline_desc.depth_stencil.depth_test = true;
        texture_pipeline_desc.depth_stencil.depth_write = true;

        builder.add_pass(
            "TexturePass",
            texture_pass_params,
            RGPassHint::Raster,
            [=, this](CommandBuffer& command, const RGPassResources resources) {
                const auto framebuffer = resources.get_framebuffer();
                command.begin_render_pass(framebuffer);
                {
                    const auto pipeline = get_graphics_pipeline(
                        texture_vertex_shader,
                        texture_fragment_shader,
                        texture_pipeline_desc.rasterization,
                        texture_pipeline_desc.depth_stencil,
                        texture_pipeline_desc.color_blend,
                        framebuffer);
                    command.bind_pipeline(pipeline);

                    std::array descriptor_set_layout = {
                        DescriptorItem::ConstantBuffer(0, 1, ShaderType::Vertex),
                        DescriptorItem::TextureSrv(0, 1, ShaderType::Fragment),
                        DescriptorItem::SamplerState(0, 1, ShaderType::Fragment),
                    };

                    std::array descriptor_set_writes = {
                        WriteDescriptor::ConstantBuffer(0, resources.get_buffer_cbv(texture_pass_params.uCameraInfo)),
                        WriteDescriptor::TextureSrv(0, resources.get_texture_srv(texture_pass_params.uTexture)),
                        WriteDescriptor::SamplerState(0, texture_pass_params.uTexture_Sampler),
                    };

                    const auto transient_descriptor_set = g_render_device->allocate_descriptor_set(
                        descriptor_set_layout, DescriptorSetAllocationType::Transient);
                    transient_descriptor_set->update(descriptor_set_writes);

                    command.bind_descriptor_set(transient_descriptor_set, 0);

                    struct ModelInfoData
                    {
                        glm::mat4 model;
                    };

                    ModelInfoData model_info{};
                    model_info.model = glm::mat4{1.0f};
                    command.push_constant(model_info);

                    command.draw_indexed(*m_cube_mesh->vertex_buffer(), *m_cube_mesh->index_buffer());
                }
                command.end_render_pass();
            });
    }

  private:
    std::shared_ptr<Mesh> m_cube_mesh;
    std::shared_ptr<BufferResource> m_camera_ubo;

    double m_time = 0.0;
};

class PlasmaGameMain : public GameMain
{
  public:
    GameDescription get_game_description() const override
    {
        GameDescription desc{};
        desc.name = "Plasma Example";
        desc.version = Version{0, 1, 0};
        desc.graphics_api = GraphicsApi::Dx12;
        desc.width = WIDTH;
        desc.height = HEIGHT;

        return desc;
    }

    GameSimulation* create_game_simulation() const override { return new PlasmaSimulation(); }

    void setup_game_renderer(GameRenderer& game_renderer) const override
    {
        game_renderer.register_module<PlasmaRenderModule>(RenderModuleLabel::Scene);
    }
};

GameMain* Mizu::create_game_main()
{
    return new PlasmaGameMain{};
}
