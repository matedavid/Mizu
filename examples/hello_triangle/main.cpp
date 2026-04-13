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
        m_byte_address_buffer = BufferUtils::create_byte_address_buffer(ba_data);

        // clang-format off
        MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(PersistentLayout)
            MIZU_DESCRIPTOR_SET_LAYOUT_CONSTANT_BUFFER(0, 1, ShaderType::Vertex)
            MIZU_DESCRIPTOR_SET_LAYOUT_SAMPLER_STATE(0, 1, ShaderType::Fragment)
            MIZU_DESCRIPTOR_SET_LAYOUT_STRUCTURED_BUFFER_SRV(0, 1, ShaderType::Fragment)
            MIZU_DESCRIPTOR_SET_LAYOUT_BYTE_ADDRESS_BUFFER_SRV(1, 1, ShaderType::Fragment)
        MIZU_END_DESCRIPTOR_SET_LAYOUT()
        // clang-format on

        std::array persistent_writes = {
            WriteDescriptor::ConstantBuffer(0, BufferResourceView::create(m_constant_buffer)),
            WriteDescriptor::SamplerState(0, get_sampler_state({})),
            WriteDescriptor::StructuredBufferSrv(0, BufferResourceView::create(m_structured_buffer)),
            WriteDescriptor::ByteAddressBufferSrv(1, BufferResourceView::create(m_byte_address_buffer)),
        };

        m_persistent_descriptor_set = g_render_device->allocate_descriptor_set(
            PersistentLayout::get_layout(), DescriptorSetAllocationType::Persistent);
        m_persistent_descriptor_set->update(persistent_writes);

        // clang-format off
        MIZU_BEGIN_DESCRIPTOR_SET_LAYOUT(BindlessLayout)
            MIZU_DESCRIPTOR_SET_LAYOUT_TEXTURE_SRV(0, BINDLESS_DESCRIPTOR_COUNT, ShaderType::Fragment)
        MIZU_END_DESCRIPTOR_SET_LAYOUT()
        // clang-format on

        std::array bindless_descriptor_set_writes = {
            WriteDescriptor::TextureSrv(0, ImageResourceView::create(m_dx12_texture)),
            WriteDescriptor::TextureSrv(0, ImageResourceView::create(m_vulkan_texture)),
        };

        m_bindless_descriptor_set = g_render_device->allocate_descriptor_set(
            BindlessLayout::get_layout(), DescriptorSetAllocationType::Bindless, 1000);
        m_bindless_descriptor_set->update(bindless_descriptor_set_writes);

        ShaderManager::get().add_shader_mapping("/HelloTriangleShaders", MIZU_ENGINE_SHADERS_PATH);
    }

    void build_render_graph(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) override
    {
        const FrameInfo& frame_info = blackboard.get<FrameInfo>();
        m_time += frame_info.last_frame_time;

        struct HelloTriangleData
        {
            RenderGraphResource output_texture;
        };

        builder.add_pass<HelloTriangleData>(
            "HelloTriangle",
            [&](RenderGraphPassBuilder& pass, HelloTriangleData& data) {
                pass.set_hint(RenderGraphPassHint::Raster);
                data.output_texture = pass.attachment(frame_info.output_texture_ref);
            },
            [=,
             this](CommandBuffer& command, const HelloTriangleData& data, const RenderGraphPassResources& resources) {
                ImageResourceViewDescription output_view_desc{};
                output_view_desc.override_format = ImageFormat::R8G8B8A8_SRGB;

                FramebufferAttachment color_attachment{};
                color_attachment.rtv =
                    ImageResourceView::create(resources.get_image(data.output_texture), output_view_desc);
                color_attachment.load_operation = LoadOperation::Clear;
                color_attachment.store_operation = StoreOperation::Store;

                RenderPassInfo pass_info{};
                pass_info.extent = {frame_info.width, frame_info.height};
                pass_info.color_attachments = {color_attachment};

                command.begin_render_pass(pass_info);
                {
                    FramebufferInfo framebuffer_info{};
                    framebuffer_info.color_attachments = {*output_view_desc.override_format};

                    DepthStencilState depth_stencil{};
                    depth_stencil.depth_test = false;
                    depth_stencil.depth_write = false;

                    const auto pipeline = get_graphics_pipeline(
                        HelloTriangleShaderVS{},
                        HelloTriangleShaderFS{},
                        RasterizationState{},
                        depth_stencil,
                        ColorBlendState{},
                        framebuffer_info);
                    command.bind_pipeline(pipeline);

                    command.bind_descriptor_set(m_persistent_descriptor_set, 0);
                    command.bind_descriptor_set(m_bindless_descriptor_set, 1);

                    struct PushConstantData
                    {
                        uint32_t textureIdx;
                        glm::vec3 _padding;
                    };

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
