#include "vulkan_swapchain.h"

#include <array>
#include <cmath>
#include <glm/glm.hpp>
#include <numeric>

#include "base/debug/assert.h"
#include "base/debug/profiling.h"

#include "render_core/resources/texture.h"

#include "render_core/rhi/rhi_window.h"

#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_framebuffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_queue.h"
#include "render_core/rhi/backend/vulkan/vulkan_resource_view.h"
#include "render_core/rhi/backend/vulkan/vulkan_synchronization.h"

namespace Mizu::Vulkan
{

VulkanSwapchain::VulkanSwapchain(SwapchainDescription desc) : m_description(std::move(desc))
{
    retrieve_surface();
    create_swapchain();
    retrieve_swapchain_images();
}

VulkanSwapchain::~VulkanSwapchain()
{
    cleanup();

    vkDestroySurfaceKHR(VulkanContext.instance->handle(), m_surface, nullptr);
}

void VulkanSwapchain::acquire_next_image(
    std::shared_ptr<Semaphore> signal_semaphore,
    std::shared_ptr<Fence> signal_fence)
{
    VkSemaphore vk_signal_semaphore = VK_NULL_HANDLE;
    VkFence vk_signal_fence = VK_NULL_HANDLE;

    if (signal_semaphore != nullptr)
    {
        vk_signal_semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(signal_semaphore)->handle();
    }

    if (signal_fence != nullptr)
    {
        vk_signal_fence = std::dynamic_pointer_cast<VulkanFence>(signal_fence)->handle();
    }

    const auto result = vkAcquireNextImageKHR(
        VulkanContext.device->handle(),
        m_swapchain,
        UINT64_MAX,
        vk_signal_semaphore,
        vk_signal_fence,
        &m_current_image_idx);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreate();
        VK_CHECK(vkAcquireNextImageKHR(
            VulkanContext.device->handle(),
            m_swapchain,
            UINT64_MAX,
            vk_signal_semaphore,
            vk_signal_fence,
            &m_current_image_idx));
    }
    else if (result != VK_SUBOPTIMAL_KHR)
    {
        VK_CHECK_RESULT(result);
    }
}

void VulkanSwapchain::present(const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores)
{
    MIZU_PROFILE_SCOPED;

    std::vector<VkSemaphore> vk_wait_semaphores;
    for (const auto& wait_semaphore : wait_semaphores)
    {
        const VkSemaphore vk_semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(wait_semaphore)->handle();
        vk_wait_semaphores.push_back(vk_semaphore);
    }

    VkPresentInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = static_cast<uint32_t>(vk_wait_semaphores.size());
    info.pWaitSemaphores = vk_wait_semaphores.data();
    info.swapchainCount = 1;
    info.pSwapchains = &m_swapchain;
    info.pImageIndices = &m_current_image_idx;

    const VkResult result = vkQueuePresentKHR(VulkanContext.device->get_graphics_queue()->handle(), &info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        recreate();
    }
    else if (result != VK_SUCCESS)
    {
        MIZU_UNREACHABLE("Failed to present image");
    }
}

std::shared_ptr<Texture2D> VulkanSwapchain::get_image(uint32_t idx) const
{
    MIZU_ASSERT(
        idx < m_images.size(), "idx is bigger than the number of swapchain images ({} >= {})", idx, m_images.size());
    return std::make_shared<Texture2D>(m_images[idx]);
}

void VulkanSwapchain::retrieve_surface()
{
    VK_CHECK(m_description.window->create_vulkan_surface(VulkanContext.instance->handle(), m_surface));
}

void VulkanSwapchain::create_swapchain()
{
    retrieve_swapchain_information();

    constexpr std::array view_format_list = {
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
    };

    VkImageFormatListCreateInfo image_format_list_create_info{};
    image_format_list_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO;
    image_format_list_create_info.viewFormatCount = static_cast<uint32_t>(view_format_list.size());
    image_format_list_create_info.pViewFormats = view_format_list.data();

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;

    create_info.flags = VK_SWAPCHAIN_CREATE_MUTABLE_FORMAT_BIT_KHR;
    create_info.pNext = &image_format_list_create_info;

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

void VulkanSwapchain::retrieve_swapchain_images()
{
    MIZU_ASSERT(m_images.empty(), "Image vector should be empty");

    // Retrieve images
    uint32_t image_count;
    VK_CHECK(vkGetSwapchainImagesKHR(VulkanContext.device->handle(), m_swapchain, &image_count, nullptr));

    std::vector<VkImage> images(image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(VulkanContext.device->handle(), m_swapchain, &image_count, images.data()));

    for (size_t i = 0; i < image_count; ++i)
    {
        constexpr ImageUsageBits swapchain_image_usage = ImageUsageBits::Attachment;

        const auto image = std::make_shared<VulkanImageResource>(
            m_swapchain_info.extent.width,
            m_swapchain_info.extent.height,
            m_description.format,
            swapchain_image_usage,
            images[i],
            false);
        m_images.push_back(image);
    }
}

void VulkanSwapchain::recreate()
{
    Renderer::wait_idle();
    cleanup();

    create_swapchain();
    retrieve_swapchain_images();
}

void VulkanSwapchain::cleanup()
{
    m_images.clear();

    // Destroy swapchain
    vkDestroySwapchainKHR(VulkanContext.device->handle(), m_swapchain, nullptr);
}

void VulkanSwapchain::retrieve_swapchain_information()
{
    m_swapchain_info = SwapchainInformation{};

    // Capabilities
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        VulkanContext.device->physical_device(), m_surface, &m_swapchain_info.capabilities));

    // Extent
    if (m_swapchain_info.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        m_swapchain_info.extent = m_swapchain_info.capabilities.currentExtent;
    }
    else
    {
        // Need this condition because of currentExtent can be 0xFFFFFFFF:
        // https://registry.khronos.org/vulkan/specs/latest/man/html/VkSurfaceCapabilitiesKHR.html

        VkExtent2D actualExtent = {
            m_description.window->get_width(),
            m_description.window->get_height(),
        };

        actualExtent.width = glm::clamp(
            actualExtent.width,
            m_swapchain_info.capabilities.minImageExtent.width,
            m_swapchain_info.capabilities.maxImageExtent.width);
        actualExtent.height = glm::clamp(
            actualExtent.height,
            m_swapchain_info.capabilities.minImageExtent.height,
            m_swapchain_info.capabilities.maxImageExtent.height);

        m_swapchain_info.extent.width = actualExtent.width;
        m_swapchain_info.extent.height = actualExtent.height;
    }

    // Surface formats
    uint32_t surface_format_count;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        VulkanContext.device->physical_device(), m_surface, &surface_format_count, nullptr));

    MIZU_ASSERT(surface_format_count > 0, "No surface formats supported");

    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        VulkanContext.device->physical_device(), m_surface, &surface_format_count, surface_formats.data()));

    // TODO: Make surface format selection configurable?
    m_swapchain_info.surface_format = surface_formats[0];

    for (const auto& format : surface_formats)
    {
        if (format.format == VulkanImageResource::get_vulkan_image_format(m_description.format)
            && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            m_swapchain_info.surface_format = format;
            break;
        }
    }

    MIZU_ASSERT(
        m_swapchain_info.surface_format.format == VulkanImageResource::get_vulkan_image_format(m_description.format),
        "Could not select requested surface format");

    // Present mode
    uint32_t present_mode_count;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        VulkanContext.device->physical_device(), m_surface, &present_mode_count, nullptr));

    MIZU_ASSERT(present_mode_count > 0, "No present modes supported");

    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        VulkanContext.device->physical_device(), m_surface, &present_mode_count, present_modes.data()));

    // TODO: Make present mode selection configurable?
    m_swapchain_info.present_mode = present_modes[0];

    for (const auto& present_mode : present_modes)
    {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            m_swapchain_info.present_mode = present_mode;
            break;
        }
    }
}

} // namespace Mizu::Vulkan
