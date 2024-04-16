#include <cassert>
#include <iostream>

#include <Mizu/Mizu.h>
#include <vulkan/vulkan.h>

#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_device.h"

class PruebasRenderer {
  public:
    PruebasRenderer() {
        Mizu::ImageDescription color_desc{};
        color_desc.width = 960;
        color_desc.height = 480;
        color_desc.format = Mizu::ImageFormat::RGBA8_SRGB;
        color_desc.attachment = true;

        m_color_texture = Mizu::Texture2D::create(color_desc);

        Mizu::Framebuffer::Attachment color_attachment{};
        color_attachment.image = m_color_texture;
        color_attachment.load_operation = Mizu::LoadOperation::DontCare;
        color_attachment.store_operation = Mizu::StoreOperation::Store;
        color_attachment.clear_value = glm::vec3(0.0f);

        Mizu::Framebuffer::Description framebuffer_desc{};
        framebuffer_desc.width = 960;
        framebuffer_desc.height = 480;
        framebuffer_desc.attachments = {color_attachment};

        m_framebuffer = Mizu::Framebuffer::create(framebuffer_desc);

        const auto shader = Mizu::Shader::create("vertex.spv", "fragment.spv");

        Mizu::GraphicsPipeline::Description pipeline_desc{};
        pipeline_desc.shader = shader;
        pipeline_desc.target_framebuffer = m_framebuffer;
        pipeline_desc.depth_stencil = Mizu::DepthStencilState{
            .depth_test = false,
            .depth_write = false,
        };

        m_pipeline = Mizu::GraphicsPipeline::create(pipeline_desc);

        // TODO: Should be abstracted:
        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        vkCreateFence(Mizu::Vulkan::VulkanContext.device->handle(), &fence_create_info, nullptr, &m_flight_fence);
    }

    ~PruebasRenderer() { vkDestroyFence(Mizu::Vulkan::VulkanContext.device->handle(), m_flight_fence, nullptr); }

    void render() {
        vkWaitForFences(Mizu::Vulkan::VulkanContext.device->handle(), 1, &m_flight_fence, VK_TRUE, UINT64_MAX);
        vkResetFences(Mizu::Vulkan::VulkanContext.device->handle(), 1, &m_flight_fence);
    }

  private:
    std::shared_ptr<Mizu::Texture2D> m_color_texture;
    std::shared_ptr<Mizu::Framebuffer> m_framebuffer;

    std::shared_ptr<Mizu::GraphicsPipeline> m_pipeline;

    VkFence m_flight_fence;
};

int main() {
    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.requirements = Mizu::Requirements{
        .graphics = true,
        .compute = false,
    };

    config.application_name = "Pruebas";
    config.application_version = Mizu::Version{0, 1, 0};

    bool ok = Mizu::initialize(config);
    if (!ok) {
        std::cout << "Failed to initialize Mizu\n";
        return 1;
    }

    {
        PruebasRenderer pruebas{};

        while (true) {
            pruebas.render();
        }
    }

    Mizu::shutdown();
    return 0;
}
