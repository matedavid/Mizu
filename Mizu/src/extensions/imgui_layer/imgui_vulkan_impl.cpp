#include "imgui_vulkan_impl.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <utility>

#include "core/application.h"
#include "core/window.h"

#include "render_core/rhi/renderer.h"

#include "render_core/resources/texture.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_device.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_instance.h"
#include "render_core/rhi/backend/vulkan/vulkan_queue.h"

namespace Mizu
{

ImGuiVulkanImpl::ImGuiVulkanImpl(std::shared_ptr<Window> window) : m_window(std::move(window))
{
    m_wd = new ImGui_ImplVulkanH_Window();
    m_window->create_vulkan_surface(Vulkan::VulkanContext.instance->handle(), m_wd->Surface);

    // Select Surface Format
    const VkFormat request_surface_formats[] = {
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    m_wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(Vulkan::VulkanContext.device->physical_device(),
                                                                m_wd->Surface,
                                                                request_surface_formats,
                                                                (size_t)IM_ARRAYSIZE(request_surface_formats),
                                                                requestSurfaceColorSpace);
    // Select Present Mode
    VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
    m_wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        Vulkan::VulkanContext.device->physical_device(), m_wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));

    create_resize_window();

    // Descriptor pool
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 100},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100},
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 10;
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(Vulkan::VulkanContext.device->handle(), &pool_info, nullptr, &m_descriptor_pool));

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(Application::instance()->get_window()->handle(), true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = Vulkan::VulkanContext.instance->handle();
    init_info.PhysicalDevice = Vulkan::VulkanContext.device->physical_device();
    init_info.Device = Vulkan::VulkanContext.device->handle();
    init_info.QueueFamily = Vulkan::VulkanContext.device->get_graphics_queue()->family();
    init_info.Queue = Vulkan::VulkanContext.device->get_graphics_queue()->handle();
    init_info.DescriptorPool = m_descriptor_pool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = m_wd->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.RenderPass = m_wd->RenderPass;
    init_info.CheckVkResultFn = [](VkResult result) {
        VK_CHECK(result);
    };
    ImGui_ImplVulkan_Init(&init_info);

    // Upload fonts
    ImGui_ImplVulkan_CreateFontsTexture();
}

ImGuiVulkanImpl::~ImGuiVulkanImpl()
{
    Renderer::wait_idle();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    vkDestroyDescriptorPool(Vulkan::VulkanContext.device->handle(), m_descriptor_pool, nullptr);

    ImGui_ImplVulkanH_DestroyWindow(
        Vulkan::VulkanContext.instance->handle(), Vulkan::VulkanContext.device->handle(), m_wd, nullptr);
}

void ImGuiVulkanImpl::new_frame()
{
    if (m_rebuild_swapchain)
    {
        const auto width = m_window->get_width();
        const auto height = m_window->get_width();

        if (width > 0 && height > 0)
        {
            ImGui_ImplVulkan_SetMinImageCount(m_min_image_count);
            create_resize_window();

            m_wd->FrameIndex = 0;
            m_rebuild_swapchain = false;
        }
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
}

void ImGuiVulkanImpl::render_frame(ImDrawData* draw_data)
{
    const VkDevice& device = Vulkan::VulkanContext.device->handle();

    ImGui_ImplVulkanH_Frame* fd = &m_wd->Frames[m_wd->FrameIndex];
    {
        VK_CHECK(vkWaitForFences(
            device, 1, &fd->Fence, VK_TRUE, UINT64_MAX)); // wait indefinitely instead of periodically checking
        VK_CHECK(vkResetFences(device, 1, &fd->Fence));
    }

    VkSemaphore image_acquired_semaphore = m_wd->FrameSemaphores[m_wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = m_wd->FrameSemaphores[m_wd->SemaphoreIndex].RenderCompleteSemaphore;
    auto err = vkAcquireNextImageKHR(
        device, m_wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &m_wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        m_rebuild_swapchain = true;
        return;
    }
    VK_CHECK(err);

    {
        VK_CHECK(vkResetCommandPool(device, fd->CommandPool, 0));
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VK_CHECK(vkBeginCommandBuffer(fd->CommandBuffer, &info));
    }

    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = m_wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = static_cast<uint32_t>(m_wd->Width);
        info.renderArea.extent.height = static_cast<uint32_t>(m_wd->Height);
        info.clearValueCount = 1;
        info.pClearValues = &m_wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear ImGui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        VK_CHECK(err);
        err = vkQueueSubmit(Vulkan::VulkanContext.device->get_graphics_queue()->handle(), 1, &info, fd->Fence);
        VK_CHECK(err);
    }
}

void ImGuiVulkanImpl::present_frame()
{
    if (m_rebuild_swapchain)
        return;

    VkSemaphore render_complete_semaphore = m_wd->FrameSemaphores[m_wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &m_wd->Swapchain;
    info.pImageIndices = &m_wd->FrameIndex;

    auto err = vkQueuePresentKHR(Vulkan::VulkanContext.device->get_graphics_queue()->handle(), &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        m_rebuild_swapchain = true;
        return;
    }
    VK_CHECK(err);
    m_wd->SemaphoreIndex = (m_wd->SemaphoreIndex + 1) % m_wd->ImageCount; // Now we can use the next set of semaphores
}

ImTextureID ImGuiVulkanImpl::add_texture(const Texture2D& texture)
{
    const auto& native_resource = std::dynamic_pointer_cast<Vulkan::VulkanImageResource>(texture.get_resource());

    return (ImTextureID)ImGui_ImplVulkan_AddTexture(
        native_resource->get_sampler(), native_resource->get_image_view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ImGuiVulkanImpl::remove_texture(ImTextureID texture_id)
{
    ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)texture_id);
}

void ImGuiVulkanImpl::create_resize_window()
{
    const auto graphics_queue = Vulkan::VulkanContext.device->get_graphics_queue();
    ImGui_ImplVulkanH_CreateOrResizeWindow(Vulkan::VulkanContext.instance->handle(),
                                           Vulkan::VulkanContext.device->physical_device(),
                                           Vulkan::VulkanContext.device->handle(),
                                           m_wd,
                                           graphics_queue->family(),
                                           nullptr,
                                           static_cast<int32_t>(m_window->get_width()),
                                           static_cast<int32_t>(m_window->get_height()),
                                           m_min_image_count);
}

} // namespace Mizu
