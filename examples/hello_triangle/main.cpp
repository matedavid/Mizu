#include <Mizu/Mizu.h>

#include <vector>

#include "hello_triangle_shaders.h"

using namespace Mizu;

static uint32_t WIDTH = 1920;
static uint32_t HEIGHT = 1080;

struct ConstantBufferData
{
    glm::vec4 colorMask;
};

class HelloTriangleSimulation : public GameSimulation
{
  public:
    void init() {}

    void update([[maybe_unused]] double dt) {}
};

class HelloTriangleRenderModule : public IRenderModule
{
  public:
    HelloTriangleRenderModule()
    {
        struct Vertex
        {
            glm::vec3 pos;
            glm::vec2 tex_coords;
            glm::vec3 color;
        };

        if (g_render_device->get_api() == GraphicsApi::Dx12)
        {
            // clang-format off
            std::vector<Vertex> vertex_data = {
                {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
                {{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                {{ 0.0f,  0.5f, 0.0f}, {0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            };
            // clang-format on

            m_vertex_buffer =
                BufferUtils::create_vertex_buffer(std::span<const Vertex>(vertex_data), "TriangleVertexBuffer");
        }
        else if (g_render_device->get_api() == GraphicsApi::Vulkan)
        {
            // clang-format off
            std::vector<Vertex> vertex_data = {
                {{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
                {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                {{ 0.0f, -0.5f, 0.0f}, {0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            };
            // clang-format on

            m_vertex_buffer =
                BufferUtils::create_vertex_buffer(std::span<const Vertex>(vertex_data), "TriangleVertexBuffer");
        }

        m_dx12_texture = ImageUtils::create_texture2d(std::filesystem::path(MIZU_EXAMPLE_PATH) / "dx12_logo.jpg");
        m_vulkan_texture = ImageUtils::create_texture2d(std::filesystem::path(MIZU_EXAMPLE_PATH) / "vulkan_logo.jpg");

        m_constant_buffer = BufferUtils::create_constant_buffer<ConstantBufferData>(ConstantBufferData{});

        ConstantBufferData data{};
        data.colorMask = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
        m_constant_buffer->set_data(reinterpret_cast<const uint8_t*>(&data), sizeof(ConstantBufferData), 0);

        const std::vector<float> sb_data = {1.0f};
        m_structured_buffer = BufferUtils::create_structured_buffer(std::span(sb_data));

        const std::vector<uint32_t> ba_data = {1};

        BufferDescription byte_address_buffer_desc{};
        byte_address_buffer_desc.size = sizeof(uint32_t) * 1;
        byte_address_buffer_desc.stride = 0;
        byte_address_buffer_desc.usage = BufferUsageBits::TransferDst;

        m_byte_address_buffer = g_render_device->create_buffer(byte_address_buffer_desc);
        BufferUtils::initialize_buffer(
            *m_byte_address_buffer,
            reinterpret_cast<const uint8_t*>(ba_data.data()),
            ba_data.size() * sizeof(uint32_t));

        std::array persistent_layout = {
            DescriptorItem::ConstantBuffer(0, 1, ShaderType::Vertex),
            DescriptorItem::SamplerState(0, 1, ShaderType::Fragment),
            DescriptorItem::StructuredBufferSrv(0, 1, ShaderType::Fragment),
            DescriptorItem::ByteAddressBufferSrv(1, 1, ShaderType::Fragment),
        };

        std::array persistent_writes = {
            WriteDescriptor::ConstantBuffer(0, m_constant_buffer->as_cbv()),
            WriteDescriptor::SamplerState(0, get_sampler_state({})),
            WriteDescriptor::StructuredBufferSrv(0, m_structured_buffer->as_srv()),
            WriteDescriptor::ByteAddressBufferSrv(1, m_byte_address_buffer->as_srv()),
        };

        m_persistent_descriptor_set =
            g_render_device->allocate_descriptor_set(persistent_layout, DescriptorSetAllocationType::Persistent);
        m_persistent_descriptor_set->update(persistent_writes);

        std::array bindless_descriptor_set_layout = {
            DescriptorItem::TextureSrv(0, 1000, ShaderType::Fragment),
        };

        std::array bindless_descriptor_set_writes = {
            WriteDescriptor::TextureSrv(0, m_dx12_texture->as_srv()),
            WriteDescriptor::TextureSrv(0, m_vulkan_texture->as_srv()),
        };

        m_bindless_descriptor_set = g_render_device->allocate_descriptor_set(
            bindless_descriptor_set_layout, DescriptorSetAllocationType::Bindless);
        m_bindless_descriptor_set->update(bindless_descriptor_set_writes);

        ShaderManager::get().add_shader_mapping("/HelloTriangleShaders", MIZU_ENGINE_SHADERS_PATH);
    }

    void build_render_graph(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
    {
        const FrameInfo& frame_info = blackboard.get<FrameInfo>();

        m_time += frame_info.last_frame_time;

        // clang-format off
        BEGIN_SHADER_PARAMETERS(Parameters)
            SHADER_PARAMETER_RG_FRAMEBUFFER_ATTACHMENTS()
        END_SHADER_PARAMETERS()
        // clang-format on

        ImageResourceViewDescription output_view_desc{};
        output_view_desc.override_format = ImageFormat::R8G8B8A8_SRGB;
        const RGTextureRtvRef rtv_ref = builder.create_texture_rtv(frame_info.output_texture_ref);

        Parameters parameters{};
        parameters.framebuffer = RGFramebufferAttachments{
            .width = frame_info.width,
            .height = frame_info.height,
            .color_attachments = {rtv_ref},
        };

        builder.add_pass(
            "HelloTriangle",
            parameters,
            RGPassHint::Raster,
            [&](CommandBuffer& command, const RGPassResources& resources) {
                const auto framebuffer = resources.get_framebuffer();

                command.begin_render_pass(framebuffer);
                {
                    HelloTriangleShaderVS vertex_shader;
                    HelloTriangleShaderFS fragment_shader;

                    struct PushConstantData
                    {
                        uint32_t textureIdx;
                        glm::vec3 _padding;
                    };

                    DepthStencilState depth_stencil{};
                    depth_stencil.depth_test = false;
                    depth_stencil.depth_write = false;

                    const auto pipeline = get_graphics_pipeline(
                        vertex_shader,
                        fragment_shader,
                        RasterizationState{},
                        depth_stencil,
                        ColorBlendState{},
                        command.get_active_framebuffer());

                    command.bind_pipeline(pipeline);
                    command.bind_descriptor_set(m_persistent_descriptor_set, 0);
                    command.bind_descriptor_set(m_bindless_descriptor_set, 1);

                    PushConstantData constant_data{};
                    constant_data.textureIdx = (glm::cos((float)m_time) + 1.0f) / 2.0f > 0.5f ? 1 : 0;
                    command.push_constant(constant_data);

                    command.draw(*m_vertex_buffer);
                }
                command.end_render_pass();
            });
    }

  private:
    std::shared_ptr<BufferResource> m_vertex_buffer;

    std::shared_ptr<DescriptorSet> m_persistent_descriptor_set;
    std::shared_ptr<DescriptorSet> m_bindless_descriptor_set;

    std::shared_ptr<ImageResource> m_dx12_texture;
    std::shared_ptr<ImageResource> m_vulkan_texture;

    std::shared_ptr<BufferResource> m_constant_buffer;
    std::shared_ptr<BufferResource> m_structured_buffer;
    std::shared_ptr<BufferResource> m_byte_address_buffer;

    double m_time = 0.0;
};

class HelloTriangleMain : public GameMain
{
  public:
    GameDescription get_game_description() const
    {
        GameDescription desc{};
        desc.name = "HelloTriangle Example";
        desc.version = Version{0, 1, 0};
        desc.graphics_api = GraphicsApi::Vulkan;
        desc.width = WIDTH;
        desc.height = HEIGHT;

        return desc;
    }

    GameSimulation* create_game_simulation() const { return new HelloTriangleSimulation{}; }

    virtual void setup_game_renderer(GameRenderer& game_renderer) const
    {
        game_renderer.register_module<HelloTriangleRenderModule>(RenderModuleLabel::Scene);
    }
};

GameMain* Mizu::create_game_main()
{
    return new HelloTriangleMain{};
}