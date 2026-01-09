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
        ShaderManager::get().add_shader_mapping("/HelloTriangleShaders", MIZU_EXAMPLE_SHADERS_PATH);

        m_command_buffer = g_render_device->create_command_buffer(CommandBufferType::Graphics);

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

        SwapchainDescription swapchain_desc{};
        swapchain_desc.window = Application::instance()->get_window();
        swapchain_desc.format = ImageFormat::R8G8B8A8_UNORM;

        m_swapchain = g_render_device->create_swapchain(swapchain_desc);

        m_fence = g_render_device->create_fence();
        m_image_acquired_semaphore = g_render_device->create_semaphore();
        m_render_finished_semaphore = g_render_device->create_semaphore();

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

        const ResourceView cbv = m_constant_buffer->as_cbv();
        const ResourceView sb_srv = m_structured_buffer->as_srv();
        const ResourceView ba_srv = m_byte_address_buffer->as_srv();

        if (g_render_device->get_api() == GraphicsApi::Dx12)
        {
            std::array persistent_layout = {
                DescriptorItem::ConstantBuffer(0, 1, ShaderType::Vertex),
                DescriptorItem::SamplerState(0, 1, ShaderType::Fragment),
                DescriptorItem::StructuredBufferSrv(1, 1, ShaderType::Fragment),
                DescriptorItem::ByteAddressBufferSrv(2, 1, ShaderType::Fragment),
            };

            std::array persistent_writes = {
                WriteDescriptor{.binding = 0, .type = ShaderResourceType::ConstantBuffer, .value = cbv},
                WriteDescriptor{.binding = 0, .type = ShaderResourceType::SamplerState, .value = get_sampler_state({})},
                WriteDescriptor{.binding = 1, .type = ShaderResourceType::StructuredBufferSrv, .value = sb_srv},
                WriteDescriptor{.binding = 2, .type = ShaderResourceType::ByteAddressBufferSrv, .value = ba_srv},
            };

            m_persistent_descriptor_set =
                g_render_device->allocate_descriptor_set(persistent_layout, DescriptorSetAllocationType::Persistent);
            m_persistent_descriptor_set->update(persistent_writes);
        }
        else if (g_render_device->get_api() == GraphicsApi::Vulkan)
        {
            std::array persistent_layout = {
                DescriptorItem::ConstantBuffer(2, 1, ShaderType::Vertex),
                DescriptorItem::SamplerState(1, 1, ShaderType::Fragment),
                DescriptorItem::StructuredBufferSrv(3, 1, ShaderType::Fragment),
                DescriptorItem::ByteAddressBufferSrv(4, 1, ShaderType::Fragment),
            };

            std::array persistent_writes = {
                WriteDescriptor{.binding = 2, .type = ShaderResourceType::ConstantBuffer, .value = cbv},
                WriteDescriptor{.binding = 1, .type = ShaderResourceType::SamplerState, .value = get_sampler_state({})},
                WriteDescriptor{.binding = 3, .type = ShaderResourceType::StructuredBufferSrv, .value = sb_srv},
                WriteDescriptor{.binding = 4, .type = ShaderResourceType::ByteAddressBufferSrv, .value = ba_srv},
            };

            m_persistent_descriptor_set =
                g_render_device->allocate_descriptor_set(persistent_layout, DescriptorSetAllocationType::Persistent);
            m_persistent_descriptor_set->update(persistent_writes);
        }

        std::array bindless_descriptor_set_layout = {
            DescriptorItem::TextureSrv(0, 1000, ShaderType::Fragment),
        };
        std::array bindless_descriptor_set_writes = {
            WriteDescriptor{.binding = 0, .type = ShaderResourceType::TextureSrv, .value = m_dx12_texture->as_srv()},
            WriteDescriptor{.binding = 0, .type = ShaderResourceType::TextureSrv, .value = m_vulkan_texture->as_srv()},
        };

        m_bindless_descriptor_set = g_render_device->allocate_descriptor_set(
            bindless_descriptor_set_layout, DescriptorSetAllocationType::Bindless);
        m_bindless_descriptor_set->update(bindless_descriptor_set_writes);
    }

    ~ExampleLayer() override { g_render_device->wait_idle(); }

    void on_update([[maybe_unused]] double ts) override
    {
        m_fence->wait_for();

        g_render_device->reset_transient_descriptors();

        static double time = 0;
        time += ts;

        ConstantBufferData data{};
        data.colorMask = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        m_constant_buffer->set_data(reinterpret_cast<const uint8_t*>(&data), sizeof(ConstantBufferData), 0);

        /*
        std::array transient_descriptor_set_layout = {
            DescriptorItem::TextureSrv(0, 1, ShaderType::Fragment),
        };

        std::array transient_descriptor_set_writes = {
            WriteDescriptor{.binding = 0, .type = ShaderResourceType::TextureSrv, .value = m_vulkan_texture->as_srv()},
        };

        const auto transient_descriptor_set = g_render_device->allocate_descriptor_set(
            transient_descriptor_set_layout, DescriptorSetAllocationType::Transient);
        transient_descriptor_set->update(transient_descriptor_set_writes);
        */

        m_swapchain->acquire_next_image(m_image_acquired_semaphore, nullptr);
        const std::shared_ptr<ImageResource>& texture = m_swapchain->get_image(m_swapchain->get_current_image_idx());

        FramebufferDescription framebuffer_desc{};
        framebuffer_desc.width = texture->get_width();
        framebuffer_desc.height = texture->get_height();
        framebuffer_desc.color_attachments = {
            FramebufferAttachment{
                .rtv = texture->as_rtv(ImageResourceViewDescription{.override_format = ImageFormat::R8G8B8A8_SRGB}),
                .load_operation = LoadOperation::Clear,
                .store_operation = StoreOperation::Store,
                .initial_state = ImageResourceState::ColorAttachment,
                .final_state = ImageResourceState::ColorAttachment,
            },
        };
        m_framebuffer = g_render_device->create_framebuffer(framebuffer_desc);

        CommandBuffer& command = *m_command_buffer;

        command.begin();
        {
            command.transition_resource(*texture, ImageResourceState::Undefined, ImageResourceState::ColorAttachment);

            command.begin_render_pass(m_framebuffer);

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
            constant_data.textureIdx = (glm::cos((float)time) + 1.0f) / 2.0f > 0.5f ? 1 : 0;
            command.push_constant(constant_data);

            command.draw(*m_vertex_buffer);

            command.end_render_pass();

            command.transition_resource(*texture, ImageResourceState::ColorAttachment, ImageResourceState::Present);
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
    std::shared_ptr<BufferResource> m_vertex_buffer;
    std::shared_ptr<Framebuffer> m_framebuffer;
    std::shared_ptr<Swapchain> m_swapchain;

    std::shared_ptr<DescriptorSet> m_persistent_descriptor_set;
    std::shared_ptr<DescriptorSet> m_bindless_descriptor_set;

    std::shared_ptr<ImageResource> m_dx12_texture;
    std::shared_ptr<ImageResource> m_vulkan_texture;

    std::shared_ptr<BufferResource> m_constant_buffer;
    std::shared_ptr<BufferResource> m_structured_buffer;
    std::shared_ptr<BufferResource> m_byte_address_buffer;

    std::shared_ptr<Fence> m_fence;
    std::shared_ptr<Semaphore> m_image_acquired_semaphore, m_render_finished_semaphore;
};

int main()
{
    constexpr GraphicsApi graphics_api = GraphicsApi::Vulkan;

    std::string app_name_suffix;
    if constexpr (graphics_api == GraphicsApi::Dx12)
        app_name_suffix = " Dx12";
    else if constexpr (graphics_api == GraphicsApi::Vulkan)
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