#include "vulkan_swapchain.h"

#include <algorithm>
#include <array>
#include <glm/glm.hpp>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"
#include "base/debug/profiling.h"
#include "render_core/definitions/rhi_window.h"

#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_debug.h"
#include "vulkan_image_resource.h"
#include "vulkan_queue.h"
#include "vulkan_synchronization.h"
#include "vulkan_types.h"

namespace Mizu::Vulkan
{

VulkanSwapchain::VulkanSwapchain(SwapchainDescription desc) : m_description(std::move(desc))
{
    m_image_ready_fence = std::make_unique<VulkanFence>(0);

    retrieve_surface();
    create_swapchain();
    retrieve_swapchain_images();
    create_semaphores();
}

VulkanSwapchain::~VulkanSwapchain()
{
    cleanup();
    destroy_semaphores();

    vkDestroySurfaceKHR(VulkanContext.device->get_instance(), m_surface, nullptr);
}

SyncPoint VulkanSwapchain::acquire_next_image()
{
    MIZU_PROFILE_SCOPED;

    const auto next_acquire_semaphore = [this]() -> AcquireSemaphore& {
        AcquireSemaphore& acquire = m_acquire_semaphores[m_acquire_semaphore_idx];
        m_acquire_semaphore_idx = (m_acquire_semaphore_idx + 1) % static_cast<uint32_t>(m_acquire_semaphores.size());

        // A binary semaphore can't be reused until the submission waiting on it has executed
        m_image_ready_fence->wait(acquire.image_ready_value);
        return acquire;
    };

    AcquireSemaphore* acquire = &next_acquire_semaphore();

    const VkResult result = vkAcquireNextImageKHR(
        VulkanContext.device->handle(),
        m_swapchain,
        UINT64_MAX,
        acquire->semaphore,
        VK_NULL_HANDLE,
        &m_current_image_idx);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreate();

        acquire = &next_acquire_semaphore();
        VK_CHECK(vkAcquireNextImageKHR(
            VulkanContext.device->handle(),
            m_swapchain,
            UINT64_MAX,
            acquire->semaphore,
            VK_NULL_HANDLE,
            &m_current_image_idx));
    }
    else if (result != VK_SUBOPTIMAL_KHR)
    {
        VK_CHECK_RESULT(result);
    }

    inplace_vector<VkSemaphoreSubmitInfo, 2> waits;
    waits.push_back(vulkan_semaphore_submit_info(acquire->semaphore, 0));
    if (m_image_ready_value > 0)
        waits.push_back(vulkan_semaphore_submit_info(m_image_ready_fence->handle(), m_image_ready_value));

    m_image_ready_value += 1;
    acquire->image_ready_value = m_image_ready_value;

    const std::array signals = {vulkan_semaphore_submit_info(m_image_ready_fence->handle(), m_image_ready_value)};

    VulkanContext.device->get_native_queue(CommandBufferType::Graphics)
        .submit_interop(std::span<const VkSemaphoreSubmitInfo>(waits.data(), waits.size()), signals);

    return SyncPoint{m_image_ready_fence.get(), m_image_ready_value};
}

void VulkanSwapchain::present(const SyncPoint& rendering_done)
{
    MIZU_PROFILE_SCOPED;

    VulkanQueue& queue = VulkanContext.device->get_native_queue(CommandBufferType::Graphics);
    const VkSemaphore present_semaphore = m_present_semaphores[m_current_image_idx];

    inplace_vector<VkSemaphoreSubmitInfo, 1> waits;
    if (rendering_done.is_valid())
        waits.push_back(vulkan_semaphore_submit_info(rendering_done));

    const std::array signals = {vulkan_semaphore_submit_info(present_semaphore, 0)};

    queue.submit_interop(std::span<const VkSemaphoreSubmitInfo>(waits.data(), waits.size()), signals);

    VkPresentInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &present_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &m_swapchain;
    info.pImageIndices = &m_current_image_idx;

    const VkResult result = queue.present(info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        recreate();
    }
    else if (result != VK_SUCCESS)
    {
        MIZU_UNREACHABLE("Failed to present image");
    }
}

std::shared_ptr<ImageResource> VulkanSwapchain::get_image(uint32_t idx) const
{
    MIZU_ASSERT(
        idx < m_images.size(), "idx is bigger than the number of swapchain images ({} >= {})", idx, m_images.size());
    return m_images[idx];
}

void VulkanSwapchain::retrieve_surface()
{
    m_description.window->create_vulkan_surface(VulkanContext.device->get_instance(), m_surface);
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
    create_info.minImageCount = select_image_count();
    create_info.imageFormat = m_swapchain_info.surface_format.format;
    create_info.imageColorSpace = m_swapchain_info.surface_format.colorSpace;
    create_info.imageExtent = m_swapchain_info.extent;
    create_info.imageArrayLayers = 1;
    // The format may be incorrect, but it only matters that it's not a depth format
    create_info.imageUsage = get_vulkan_image_usage(m_description.usage, ImageFormat::R8G8B8A8_SRGB);

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

uint32_t VulkanSwapchain::select_image_count() const
{
    const VkSurfaceCapabilitiesKHR& capabilities = m_swapchain_info.capabilities;

    uint32_t image_count = m_description.desired_image_count;
    if (image_count == 0)
    {
        image_count = capabilities.minImageCount + 1;
    }

    image_count = std::max(image_count, capabilities.minImageCount);

    // A maxImageCount of 0 means there is no limit
    if (capabilities.maxImageCount > 0)
    {
        image_count = std::min(image_count, capabilities.maxImageCount);
    }

    return image_count;
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
        VK_DEBUG_SET_OBJECT_NAME(images[i], VK_OBJECT_TYPE_IMAGE, "SwapchainImage");

        const auto image = std::make_shared<VulkanImageResource>(
            m_swapchain_info.extent.width,
            m_swapchain_info.extent.height,
            m_description.format,
            m_description.usage,
            ImageFlagBits::MutableFormat,
            images[i],
            false);
        m_images.push_back(image);
    }
}

void VulkanSwapchain::recreate()
{
    VulkanContext.device->wait_idle();
    cleanup();
    destroy_semaphores();

    create_swapchain();
    retrieve_swapchain_images();
    create_semaphores();
}

void VulkanSwapchain::cleanup()
{
    // In vulkan, images are owned by swapchain, so no need to release them
    m_images.clear();

    vkDestroySwapchainKHR(VulkanContext.device->handle(), m_swapchain, nullptr);
}

void VulkanSwapchain::create_semaphores()
{
    VkSemaphoreCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    m_present_semaphores.resize(m_images.size());
    for (VkSemaphore& semaphore : m_present_semaphores)
    {
        VK_CHECK(vkCreateSemaphore(VulkanContext.device->handle(), &info, nullptr, &semaphore));
    }

    // +1 the number of images so the CPU wait before reusing one does not block
    m_acquire_semaphores.resize(m_images.size() + 1);
    for (AcquireSemaphore& acquire : m_acquire_semaphores)
    {
        VK_CHECK(vkCreateSemaphore(VulkanContext.device->handle(), &info, nullptr, &acquire.semaphore));
        acquire.image_ready_value = 0;
    }

    m_acquire_semaphore_idx = 0;
}

void VulkanSwapchain::destroy_semaphores()
{
    for (VkSemaphore semaphore : m_present_semaphores)
    {
        vkDestroySemaphore(VulkanContext.device->handle(), semaphore, nullptr);
    }
    m_present_semaphores.clear();

    for (const AcquireSemaphore& acquire : m_acquire_semaphores)
    {
        vkDestroySemaphore(VulkanContext.device->handle(), acquire.semaphore, nullptr);
    }
    m_acquire_semaphores.clear();
}

void VulkanSwapchain::retrieve_swapchain_information()
{
    m_swapchain_info = SwapchainInformation{};

    // Capabilities
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        VulkanContext.device->get_physical_device(), m_surface, &m_swapchain_info.capabilities));

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
        VulkanContext.device->get_physical_device(), m_surface, &surface_format_count, nullptr));

    MIZU_ASSERT(surface_format_count > 0, "No surface formats supported");

    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
        VulkanContext.device->get_physical_device(), m_surface, &surface_format_count, surface_formats.data()));

    // TODO: Make surface format selection configurable?
    m_swapchain_info.surface_format = surface_formats[0];

    for (const auto& format : surface_formats)
    {
        if (format.format == get_vulkan_image_format(m_description.format)
            && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            m_swapchain_info.surface_format = format;
            break;
        }
    }

    MIZU_ASSERT(
        m_swapchain_info.surface_format.format == get_vulkan_image_format(m_description.format),
        "Could not select requested surface format");

    // Present mode
    uint32_t present_mode_count;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        VulkanContext.device->get_physical_device(), m_surface, &present_mode_count, nullptr));

    MIZU_ASSERT(present_mode_count > 0, "No present modes supported");

    std::vector<VkPresentModeKHR> present_modes(present_mode_count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
        VulkanContext.device->get_physical_device(), m_surface, &present_mode_count, present_modes.data()));

    // VK_PRESENT_MODE_FIFO_KHR is required by the spec to always be supported, so it's the fallback
    m_swapchain_info.present_mode = VK_PRESENT_MODE_FIFO_KHR;

    const VkPresentModeKHR desired_present_mode = get_vulkan_present_mode(m_description.present_mode);

    for (const auto& present_mode : present_modes)
    {
        if (present_mode == desired_present_mode)
        {
            m_swapchain_info.present_mode = desired_present_mode;
            break;
        }
    }
}

} // namespace Mizu::Vulkan
