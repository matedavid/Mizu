#include "vulkan_swapchain.h"

#include <cmath>
#include <numeric>

#include "core/window.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image.h"
#include "renderer/abstraction/backend/vulkan/vulkan_queue.h"

namespace Mizu::Vulkan {

VulkanSwapchain::VulkanSwapchain(VkSurfaceKHR surface, std::shared_ptr<Window> window)
      : m_surface(surface), m_window(std::move(window)) {
    create_swapchain();
    retrieve_swapchain_images();
    create_render_pass();
    create_framebuffers();
}

VulkanSwapchain::~VulkanSwapchain() {
    cleanup();

    // Destroy render pass
    vkDestroyRenderPass(VulkanContext.device->handle(), m_render_pass, nullptr);
}

void VulkanSwapchain::acquire_next_image(VkSemaphore signal_semaphore, VkFence signal_fence) {
    const auto result = vkAcquireNextImageKHR(
        VulkanContext.device->handle(), m_swapchain, UINT64_MAX, signal_semaphore, signal_fence, &m_current_image_idx);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate();
        VK_CHECK(vkAcquireNextImageKHR(
            VulkanContext.device->handle(), m_swapchain, UINT64_MAX, signal_semaphore, signal_fence, &m_current_image_idx));
    } else if (result != VK_SUBOPTIMAL_KHR) {
        VK_CHECK(result);
    }
}

void VulkanSwapchain::recreate() {
    vkDeviceWaitIdle(VulkanContext.device->handle());
    cleanup();

    create_swapchain();
    retrieve_swapchain_images();
    create_framebuffers();
}

void VulkanSwapchain::create_swapchain() {
    retrieve_swapchain_information();

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_surface;
    create_info.minImageCount = m_swapchain_info.capabilities.minImageCount + 1;
    create_info.imageFormat = m_swapchain_info.surface_format.format;
    create_info.imageColorSpace = m_swapchain_info.surface_format.colorSpace;
    create_info.imageExtent = m_swapchain_info.extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;

    create_info.preTransform = m_swapchain_info.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = m_swapchain_info.present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(VulkanContext.device->handle(), &create_info, nullptr, &m_swapchain));
}

void VulkanSwapchain::retrieve_swapchain_images() {
    assert(m_images.empty() && m_image_views.empty() && "Images and views vectors should be empty");

    // Retrieve images
    uint32_t image_count;
    VK_CHECK(vkGetSwapchainImagesKHR(VulkanContext.device->handle(), m_swapchain, &image_count, nullptr));

    m_images.resize(image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(VulkanContext.device->handle(), m_swapchain, &image_count, m_images.data()));

    // Create image views
    m_image_views.resize(image_count);
    for (size_t i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        // TODO: For the moment hardcoded, should be m_swapchain_info.surface_format
        view_info.format = VK_FORMAT_B8G8R8A8_SRGB;

        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(VulkanContext.device->handle(), &view_info, nullptr, &m_image_views[i]));
    }

    // Create depth image
    VulkanImage::Description desc{};
    desc.width = m_swapchain_info.extent.width;
    desc.height = m_swapchain_info.extent.height;
    desc.type = VK_IMAGE_TYPE_2D;
    desc.format = ImageFormat::D32_SFLOAT;
    desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    m_depth_image = std::make_unique<VulkanImage>(desc);
}

void VulkanSwapchain::create_render_pass() {
    VkAttachmentDescription color_attachment_description{};
    color_attachment_description.format = m_swapchain_info.surface_format.format;
    color_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_attachment_description{};
    depth_attachment_description.format = VK_FORMAT_D32_SFLOAT;
    depth_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment_description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_description{};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &color_attachment_ref;
    subpass_description.pDepthStencilAttachment = &depth_attachment_ref;

    VkSubpassDependency subpass_dependency{};
    subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency.dstSubpass = 0;
    subpass_dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpass_dependency.srcAccessMask = 0;
    subpass_dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    subpass_dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::vector<VkAttachmentDescription> attachments = {color_attachment_description, depth_attachment_description};

    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_create_info.pAttachments = attachments.data();
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass_description;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &subpass_dependency;

    VK_CHECK(vkCreateRenderPass(VulkanContext.device->handle(), &render_pass_create_info, nullptr, &m_render_pass));
}

void VulkanSwapchain::create_framebuffers() {
    assert(m_framebuffers.empty() && "Framebuffer array should be empty");

    m_framebuffers.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); ++i) {
        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = m_render_pass;

        std::array<VkImageView, 2> attachments = {m_image_views[i], m_depth_image->get_image_view()};
        framebuffer_info.attachmentCount = attachments.size();
        framebuffer_info.pAttachments = attachments.data();

        framebuffer_info.width = m_swapchain_info.extent.width;
        framebuffer_info.height = m_swapchain_info.extent.height;
        framebuffer_info.layers = 1;

        VK_CHECK(vkCreateFramebuffer(VulkanContext.device->handle(), &framebuffer_info, nullptr, &m_framebuffers[i]));
    }
}

void VulkanSwapchain::cleanup() {
    // Clear image views and framebuffers
    // Images are destroyed when destroying the swapchain
    for (size_t i = 0; i < m_images.size(); ++i) {
        vkDestroyFramebuffer(VulkanContext.device->handle(), m_framebuffers[i], nullptr);
        vkDestroyImageView(VulkanContext.device->handle(), m_image_views[i], nullptr);
    }

    m_framebuffers.clear();
    m_image_views.clear();
    m_images.clear();

    // Clear depth image
    m_depth_image.reset();

    // Destroy swapchain
    vkDestroySwapchainKHR(VulkanContext.device->handle(), m_swapchain, nullptr);
}

void VulkanSwapchain::retrieve_swapchain_information() {
    m_swapchain_info = SwapchainInformation{};

    // Capabilities
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        VulkanContext.device->physical_device(), m_surface, &m_swapchain_info.capabilities));

    // Extent
    if (m_swapchain_info.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        m_swapchain_info.extent = m_swapchain_info.capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = {m_window->get_width(), m_window->get_height()};

        actualExtent.width = std::clamp(actualExtent.width,
                                        m_swapchain_info.capabilities.minImageExtent.width,
                                        m_swapchain_info.capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,
                                         m_swapchain_info.capabilities.minImageExtent.height,
                                         m_swapchain_info.capabilities.maxImageExtent.height);

        m_swapchain_info.extent.width = actualExtent.width;
        m_swapchain_info.extent.height = actualExtent.height;
    }

    // Surface formats
    uint32_t surface_format_count;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        VulkanContext.device->physical_device(), m_surface, &surface_format_count, nullptr));

    assert(surface_format_count > 0 && "No surface formats supported");

    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        VulkanContext.device->physical_device(), m_surface, &surface_format_count, surface_formats.data()));

    // TODO: Make surface format selection configurable?
    m_swapchain_info.surface_format = surface_formats[0];

    for (const auto& format : surface_formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            m_swapchain_info.surface_format = format;
            break;
        }
    }

    // Present mode
    uint32_t present_mode_count;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        VulkanContext.device->physical_device(), m_surface, &present_mode_count, nullptr));

    assert(present_mode_count > 0 && "No present modes supported");

    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        VulkanContext.device->physical_device(), m_surface, &present_mode_count, present_modes.data()));

    // TODO: Make present mode selection configurable?
    m_swapchain_info.present_mode = present_modes[0];

    for (const auto& present_mode : present_modes) {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            m_swapchain_info.present_mode = present_mode;
            break;
        }
    }
}

} // namespace Mizu::Vulkan
