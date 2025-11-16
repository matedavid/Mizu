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

class ExampleLayer : public Layer
{
  public:
    void on_init() override
    {
        ShaderManager::create_shader_mapping("/HelloTriangleShaders", MIZU_EXAMPLE_SHADERS_PATH);

        m_command_buffer = RenderCommandBuffer::create();

        struct Vertex
        {
            glm::vec3 pos;
            glm::vec2 tex_coords;
            glm::vec3 color;
        };

        std::vector<Vertex> vertex_data;
        if (Renderer::get_config().graphics_api == GraphicsAPI::DirectX12)
        {
            // clang-format off
            vertex_data = {
                {{-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
                {{ 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                {{ 0.0f,  0.5f, 0.0f}, {0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            };
            // clang-format on

            m_vertex_buffer = VertexBuffer::create(vertex_data);
        }
        else if (Renderer::get_config().graphics_api == GraphicsAPI::Vulkan)
        {
            // clang-format off
            vertex_data = {
                {{-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
                {{ 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
                {{ 0.0f, -0.5f, 0.0f}, {0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
            };
            // clang-format on
        }

        m_vertex_buffer = VertexBuffer::create(vertex_data);

        SwapchainDescription swapchain_desc{};
        swapchain_desc.window = Application::instance()->get_window();
        swapchain_desc.format = ImageFormat::RGBA8_UNORM;

        m_swapchain = Swapchain::create(swapchain_desc);

        m_fence = Fence::create();
        m_image_acquired_semaphore = Semaphore::create();
        m_render_finished_semaphore = Semaphore::create();

        m_texture = Texture2D::create(std::filesystem::path(MIZU_EXAMPLE_PATH) / "vulkan_logo.jpg");

        auto view = ImageResourceView::create(m_texture->get_resource());
        auto texture_srv = ShaderResourceView::create(m_texture->get_resource());

        m_constant_buffer = ConstantBuffer::create<ConstantBufferData>();

        ConstantBufferData data{};
        data.colorMask = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
        m_constant_buffer->update(data);

        auto cbv = ConstantBufferView::create(m_constant_buffer->get_resource());

        ResourceGroupBuilder builder{};

        if (Renderer::get_config().graphics_api == GraphicsAPI::DirectX12)
        {
            builder.add_resource(ResourceGroupItem::ConstantBuffer(0, cbv, ShaderType::Vertex));
            builder.add_resource(ResourceGroupItem::TextureSrv(0, texture_srv, ShaderType::Fragment));
            builder.add_resource(
                ResourceGroupItem::Sampler(0, RHIHelpers::get_sampler_state({}), ShaderType::Fragment));
        }
        else if (Renderer::get_config().graphics_api == GraphicsAPI::Vulkan)
        {
            builder.add_resource(ResourceGroupItem::ConstantBuffer(2, cbv, ShaderType::Vertex));
            builder.add_resource(ResourceGroupItem::TextureSrv(0, texture_srv, ShaderType::Fragment));
            builder.add_resource(
                ResourceGroupItem::Sampler(1, RHIHelpers::get_sampler_state({}), ShaderType::Fragment));
        }

        m_resource_group = ResourceGroup::create(builder);
    }

    ~ExampleLayer() override { Renderer::wait_idle(); }

    void on_update([[maybe_unused]] double ts) override
    {
        m_fence->wait_for();

        static double time = 0;
        time += ts;

        ConstantBufferData data{};
        data.colorMask = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) * glm::cos((float)time);
        m_constant_buffer->update(data);

        m_swapchain->acquire_next_image(m_image_acquired_semaphore, nullptr);
        const std::shared_ptr<Texture2D>& texture = m_swapchain->get_image(m_swapchain->get_current_image_idx());

        Framebuffer::Description framebuffer_desc{};
        framebuffer_desc.width = texture->get_resource()->get_width();
        framebuffer_desc.height = texture->get_resource()->get_height();
        framebuffer_desc.attachments = {
            Framebuffer::Attachment{
                .rtv = RenderTargetView::create(texture->get_resource(), ImageFormat::RGBA8_SRGB),
                .load_operation = LoadOperation::Clear,
                .store_operation = StoreOperation::Store,
                .initial_state = ImageResourceState::ColorAttachment,
                .final_state = ImageResourceState::ColorAttachment,
            },
        };
        m_framebuffer = Framebuffer::create(framebuffer_desc);
        m_render_pass = RenderPass::create(RenderPass::Description{.target_framebuffer = m_framebuffer});

        CommandBuffer& command = *m_command_buffer;

        command.begin();
        {
            command.transition_resource(
                *texture->get_resource(), ImageResourceState::Undefined, ImageResourceState::ColorAttachment);

            command.begin_render_pass(m_render_pass);

            HelloTriangleShaderVS vertex_shader;
            HelloTriangleShaderFS fragment_shader;

            GraphicsPipeline::Description pipeline_desc{};
            pipeline_desc.vertex_shader = vertex_shader.get_shader();
            pipeline_desc.fragment_shader = fragment_shader.get_shader();
            pipeline_desc.depth_stencil.depth_test = false;
            pipeline_desc.depth_stencil.depth_write = false;

            RHIHelpers::set_pipeline_state(command, pipeline_desc);
            command.bind_resource_group(m_resource_group, 0);

            command.draw(*m_vertex_buffer);

            command.end_render_pass();

            command.transition_resource(
                *texture->get_resource(), ImageResourceState::ColorAttachment, ImageResourceState::Present);
        }
        command.end();

        CommandBufferSubmitInfo submit_info{};
        submit_info.signal_fence = m_fence;
        submit_info.wait_semaphores = {m_image_acquired_semaphore};
        submit_info.signal_semaphores = {m_render_finished_semaphore};
        command.submit(submit_info);

        m_swapchain->present({m_render_finished_semaphore});
    }

  private:
    std::shared_ptr<CommandBuffer> m_command_buffer;
    std::shared_ptr<VertexBuffer> m_vertex_buffer;
    std::shared_ptr<Framebuffer> m_framebuffer;
    std::shared_ptr<RenderPass> m_render_pass;
    std::shared_ptr<Swapchain> m_swapchain;
    std::shared_ptr<ResourceGroup> m_resource_group;

    std::shared_ptr<Texture2D> m_texture;
    std::shared_ptr<ConstantBuffer> m_constant_buffer;

    std::shared_ptr<Fence> m_fence;
    std::shared_ptr<Semaphore> m_image_acquired_semaphore, m_render_finished_semaphore;
};

int main()
{
    constexpr GraphicsAPI graphics_api = GraphicsAPI::Vulkan;

    std::string app_name_suffix;
    if constexpr (graphics_api == GraphicsAPI::DirectX12)
        app_name_suffix = " D3D12";
    else if constexpr (graphics_api == GraphicsAPI::Vulkan)
        app_name_suffix = " Vulkan";

    Application::Description desc{};
    desc.graphics_api = graphics_api;
    desc.name = "HelloTriangle" + app_name_suffix;
    desc.width = WIDTH;
    desc.height = HEIGHT;

    Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}