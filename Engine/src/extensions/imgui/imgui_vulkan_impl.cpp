#include "imgui_vulkan_impl.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "application/application.h"
#include "application/window.h"

#include "render_core/rhi/rhi_helpers.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_device.h"
#include "render_core/rhi/backend/vulkan/vulkan_instance.h"
#include "render_core/rhi/backend/vulkan/vulkan_queue.h"
#include "render_core/rhi/backend/vulkan/vulkan_resource_view.h"
#include "render_core/rhi/backend/vulkan/vulkan_sampler_state.h"
#include "render_core/rhi/backend/vulkan/vulkan_synchronization.h"

namespace Mizu
{

constexpr uint32_t MIN_IMAGE_COUNT = 2;

ImGuiVulkanImpl::ImGuiVulkanImpl(std::shared_ptr<Window> window) : m_window(std::move(window))
{
    m_wnd = std::make_unique<ImGui_ImplVulkanH_Window>();
    m_window->create_vulkan_surface(Vulkan::VulkanContext.instance->handle(), m_wnd->Surface);

    const VkFormat request_surface_formats[] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB};
    const VkColorSpaceKHR request_surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    m_wnd->SurfaceFormat =
        ImGui_ImplVulkanH_SelectSurfaceFormat(Vulkan::VulkanContext.device->physical_device(),
                                              m_wnd->Surface,
                                              request_surface_formats,
                                              static_cast<size_t>(IM_ARRAYSIZE(request_surface_formats)),
                                              request_surface_color_space);

    const VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
    m_wnd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(Vulkan::VulkanContext.device->physical_device(),
                                                             m_wnd->Surface,
                                                             &present_modes[0],
                                                             IM_ARRAYSIZE(present_modes));

    create_resize_window();

    const VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 100},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 10;
    pool_info.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    VK_CHECK(vkCreateDescriptorPool(Vulkan::VulkanContext.device->handle(), &pool_info, nullptr, &m_descriptor_pool));

    ImGui_ImplGlfw_InitForVulkan(m_window->handle(), true);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = Vulkan::VulkanContext.instance->handle();
    init_info.PhysicalDevice = Vulkan::VulkanContext.device->physical_device();
    init_info.Device = Vulkan::VulkanContext.device->handle();
    init_info.QueueFamily = Vulkan::VulkanContext.device->get_graphics_queue()->family();
    init_info.Queue = Vulkan::VulkanContext.device->get_graphics_queue()->handle();
    init_info.DescriptorPool = m_descriptor_pool;
    init_info.Subpass = 0;
    init_info.MinImageCount = MIN_IMAGE_COUNT;
    init_info.ImageCount = m_wnd->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.RenderPass = m_wnd->RenderPass;
    init_info.CheckVkResultFn = [](VkResult result) {
        VK_CHECK(result);
    };
    ImGui_ImplVulkan_Init(&init_info);

    m_sampler = RHIHelpers::get_sampler_state(SamplingOptions{});
}

ImGuiVulkanImpl::~ImGuiVulkanImpl()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    vkDestroyDescriptorPool(Vulkan::VulkanContext.device->handle(), m_descriptor_pool, nullptr);

    ImGui_ImplVulkanH_DestroyWindow(
        Vulkan::VulkanContext.instance->handle(), Vulkan::VulkanContext.device->handle(), m_wnd.get(), nullptr);
}

void ImGuiVulkanImpl::new_frame(std::shared_ptr<Semaphore> signal_semaphore, std::shared_ptr<Fence> signal_fence)
{
    if (m_rebuild_swapchain)
    {
        const auto width = m_window->get_width();
        const auto height = m_window->get_width();

        if (width > 0 && height > 0)
        {
            ImGui_ImplVulkan_SetMinImageCount(MIN_IMAGE_COUNT);
            create_resize_window();

            m_wnd->FrameIndex = 0;
            m_rebuild_swapchain = false;
        }
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    VkSemaphore vk_signal_semaphore = VK_NULL_HANDLE;
    if (signal_semaphore != nullptr)
    {
        vk_signal_semaphore = std::dynamic_pointer_cast<Vulkan::VulkanSemaphore>(signal_semaphore)->handle();
    }

    VkFence vk_signal_fence = VK_NULL_HANDLE;
    if (signal_fence != nullptr)
    {
        vk_signal_fence = std::dynamic_pointer_cast<Vulkan::VulkanFence>(signal_fence)->handle();
    }

    const VkResult err = vkAcquireNextImageKHR(Vulkan::VulkanContext.device->handle(),
                                               m_wnd->Swapchain,
                                               UINT64_MAX,
                                               vk_signal_semaphore,
                                               vk_signal_fence,
                                               &m_wnd->FrameIndex);

    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        m_rebuild_swapchain = true;
    }

    if (err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }

    if (err != VK_SUBOPTIMAL_KHR)
    {
        VK_CHECK(err);
    }
}

void ImGuiVulkanImpl::render_frame(const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores) const
{
    const VkDevice device = Vulkan::VulkanContext.device->handle();

    const ImGui_ImplVulkanH_Frame* fd = &m_wnd->Frames[static_cast<int32_t>(m_wnd->FrameIndex)];

    VK_CHECK(vkWaitForFences(device, 1, &fd->Fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(device, 1, &fd->Fence));

    VK_CHECK(vkResetCommandPool(device, fd->CommandPool, 0));

    VkCommandBufferBeginInfo command_buffer_begin_info{};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(fd->CommandBuffer, &command_buffer_begin_info));

    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = m_wnd->RenderPass;
    render_pass_begin_info.framebuffer = fd->Framebuffer;
    render_pass_begin_info.renderArea.extent.width = static_cast<uint32_t>(m_wnd->Width);
    render_pass_begin_info.renderArea.extent.height = static_cast<uint32_t>(m_wnd->Height);
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &m_wnd->ClearValue;

    Vulkan::VK_DEBUG_BEGIN_GPU_MARKER(fd->CommandBuffer, "ImGui");

    vkCmdBeginRenderPass(fd->CommandBuffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), fd->CommandBuffer);
    }
    vkCmdEndRenderPass(fd->CommandBuffer);

    Vulkan::VK_DEBUG_END_GPU_MARKER(fd->CommandBuffer);

    std::vector<VkSemaphore> vk_wait_semaphores;
    std::vector<VkPipelineStageFlags> vk_wait_stages;

    for (const auto& wait_semaphore : wait_semaphores)
    {
        const auto native_wait_semaphore = std::dynamic_pointer_cast<Vulkan::VulkanSemaphore>(wait_semaphore);
        vk_wait_semaphores.push_back(native_wait_semaphore->handle());

        vk_wait_stages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = static_cast<uint32_t>(vk_wait_semaphores.size());
    submit_info.pWaitSemaphores = vk_wait_semaphores.data();
    submit_info.pWaitDstStageMask = vk_wait_stages.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &fd->CommandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores =
        &m_wnd->FrameSemaphores[static_cast<int32_t>(m_wnd->SemaphoreIndex)].RenderCompleteSemaphore;

    VK_CHECK(vkEndCommandBuffer(fd->CommandBuffer));

    VK_CHECK(vkQueueSubmit(Vulkan::VulkanContext.device->get_graphics_queue()->handle(), 1, &submit_info, fd->Fence));
}

void ImGuiVulkanImpl::present_frame()
{
    const VkSemaphore render_complete_semaphore =
        m_wnd->FrameSemaphores[static_cast<int32_t>(m_wnd->SemaphoreIndex)].RenderCompleteSemaphore;

    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &m_wnd->Swapchain;
    info.pImageIndices = &m_wnd->FrameIndex;

    const VkResult err = vkQueuePresentKHR(Vulkan::VulkanContext.device->get_graphics_queue()->handle(), &info);

    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        m_rebuild_swapchain = true;
    }

    if (err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }

    if (err != VK_SUBOPTIMAL_KHR)
    {
        VK_CHECK(err);
    }

    m_wnd->SemaphoreIndex = (m_wnd->SemaphoreIndex + 1) % m_wnd->ImageCount;
}

ImTextureID ImGuiVulkanImpl::add_texture(const ImageResourceView& view) const
{
    const Vulkan::VulkanImageResourceView& native_view = dynamic_cast<const Vulkan::VulkanImageResourceView&>(view);
    const Vulkan::VulkanSamplerState& native_sampler = dynamic_cast<const Vulkan::VulkanSamplerState&>(*m_sampler);

    return (ImTextureID)ImGui_ImplVulkan_AddTexture(
        native_sampler.handle(), native_view.handle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ImGuiVulkanImpl::remove_texture(ImTextureID id) const
{
    ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)id);
}

void ImGuiVulkanImpl::create_resize_window()
{
    const uint32_t graphics_queue_family = Vulkan::VulkanContext.device->get_graphics_queue()->family();

    ImGui_ImplVulkanH_CreateOrResizeWindow(Vulkan::VulkanContext.instance->handle(),
                                           Vulkan::VulkanContext.device->physical_device(),
                                           Vulkan::VulkanContext.device->handle(),
                                           m_wnd.get(),
                                           graphics_queue_family,
                                           nullptr,
                                           static_cast<int32_t>(m_window->get_width()),
                                           static_cast<int32_t>(m_window->get_height()),
                                           MIN_IMAGE_COUNT);
}

} // namespace Mizu
