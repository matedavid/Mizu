#include <Mizu/Mizu.h>

#include <vector>

using namespace Mizu;

static uint32_t WIDTH = 1920;
static uint32_t HEIGHT = 1080;

class TriangleShader : public GraphicsShaderDeclaration
{
  public:
    IMPLEMENT_GRAPHICS_SHADER_DECLARATION(
        "/ExampleShadersPath/Triangle.vert.spv",
        "vertexMain",
        "/ExampleShadersPath/Triangle.frag.spv",
        "fragmentMain")
};

class ExampleLayer : public Layer
{
  public:
    void on_init() override
    {
        ShaderManager::create_shader_mapping("/ExampleShadersPath", MIZU_EXAMPLE_SHADERS_PATH);

        m_command_buffer = RenderCommandBuffer::create();

        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 color;
        };

        // clang-format off
        const std::vector<Vertex> vertex_data = {
            {{-0.5f,  0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
            {{ 0.0f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        };
        // clang-format on
        m_vertex_buffer = VertexBuffer::create(vertex_data);

        m_swapchain = Swapchain::create(Application::instance()->get_window());

        m_fence = Fence::create();
        m_image_acquired_semaphore = Semaphore::create();
        m_render_finished_semaphore = Semaphore::create();
    }

    ~ExampleLayer() override { Renderer::wait_idle(); }

    void on_update([[maybe_unused]] double ts) override
    {
        m_fence->wait_for();

        m_swapchain->acquire_next_image(m_image_acquired_semaphore, nullptr);
        const std::shared_ptr<Texture2D>& texture = m_swapchain->get_image(m_swapchain->get_current_image_idx());

        Framebuffer::Description framebuffer_desc{};
        framebuffer_desc.width = texture->get_resource()->get_width();
        framebuffer_desc.height = texture->get_resource()->get_height();
        framebuffer_desc.attachments = {
            Framebuffer::Attachment{
                .image_view = ImageResourceView::create(texture->get_resource()),
                .load_operation = LoadOperation::Clear,
                .store_operation = StoreOperation::Store,
                .initial_state = ImageResourceState::Undefined,
                .final_state = ImageResourceState::Present,
            },
        };
        m_framebuffer = Framebuffer::create(framebuffer_desc);
        m_render_pass = RenderPass::create(RenderPass::Description{.target_framebuffer = m_framebuffer});

        CommandBuffer& command = *m_command_buffer;

        command.begin();
        {
            command.begin_render_pass(m_render_pass);

            const GraphicsPipeline::Description pipeline_desc =
                TriangleShader::get_pipeline_template(TriangleShader{}.get_shader_description());
            RHIHelpers::set_pipeline_state(command, pipeline_desc);
            command.draw(*m_vertex_buffer);

            command.end_render_pass();
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

    std::shared_ptr<Fence> m_fence;
    std::shared_ptr<Semaphore> m_image_acquired_semaphore, m_render_finished_semaphore;
};

int main()
{
    Application::Description desc{};
    desc.graphics_api = GraphicsAPI::Vulkan;
    desc.name = "HelloTriangle";
    desc.width = WIDTH;
    desc.height = HEIGHT;

    Application app{desc};
    app.push_layer<ExampleLayer>();

    app.run();

    return 0;
}