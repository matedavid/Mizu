#include "imgui_vulkan_impl.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <utility>

#include "core/application.h"
#include "core/window.h"

#include "render_core/rhi/renderer.h"
#include "render_core/rhi/rhi_helpers.h"

#include "render_core/resources/texture.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_device.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_instance.h"
#include "render_core/rhi/backend/vulkan/vulkan_queue.h"
#include "render_core/rhi/backend/vulkan/vulkan_resource_view.h"
#include "render_core/rhi/backend/vulkan/vulkan_sampler_state.h"
#include "render_core/rhi/backend/vulkan/vulkan_synchronization.h"

namespace Mizu
{

ImGuiVulkanImpl::ImGuiVulkanImpl(std::shared_ptr<Window> window) : m_window(std::move(window))
{
    m_wd = new ImGui_ImplVulkanH_Window();
    m_window->create_vulkan_surface(Vulkan::VulkanContext.instance->handle(), m_wd->Surface);

    // Select Surface Format
    const VkFormat request_surface_formats[] = {VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB};
    const VkColorSpaceKHR request_surface_color_space = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    m_wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(Vulkan::VulkanContext.device->physical_device(),
                                                                m_wd->Surface,
                                                                request_surface_formats,
                                                                (size_t)IM_ARRAYSIZE(request_surface_formats),
                                                                request_surface_color_space);
    // Select Present Mode
    VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
    m_wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        Vulkan::VulkanContext.device->physical_device(), m_wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));

    create_resize_window();

    // Descriptor pool
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 100}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
        /*
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 0},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 0},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 0},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 0},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 0},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 0},
        */
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
    init_info.MinImageCount = m_min_image_count;
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

    m_sampler = RHIHelpers::get_sampler_state(SamplingOptions{});
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

void ImGuiVulkanImpl::render_frame(ImDrawData* draw_data, std::shared_ptr<Semaphore> wait_semaphore)
{
    const VkDevice& device = Vulkan::VulkanContext.device->handle();

    const VkSemaphore image_acquired_semaphore =
        m_wd->FrameSemaphores[static_cast<int32_t>(m_wd->SemaphoreIndex)].ImageAcquiredSemaphore;
    const VkSemaphore render_complete_semaphore =
        m_wd->FrameSemaphores[static_cast<int32_t>(m_wd->SemaphoreIndex)].RenderCompleteSemaphore;

    const VkResult err = vkAcquireNextImageKHR(
        device, m_wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &m_wd->FrameIndex);

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

    const ImGui_ImplVulkanH_Frame* fd = &m_wd->Frames[static_cast<int32_t>(m_wd->FrameIndex)];
    VK_CHECK(vkWaitForFences(device, 1, &fd->Fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(device, 1, &fd->Fence));

    VK_CHECK(vkResetCommandPool(device, fd->CommandPool, 0));
    VkCommandBufferBeginInfo command_buffer_begin_info{};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(fd->CommandBuffer, &command_buffer_begin_info));

    VkRenderPassBeginInfo render_pass_begin_info{};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.renderPass = m_wd->RenderPass;
    render_pass_begin_info.framebuffer = fd->Framebuffer;
    render_pass_begin_info.renderArea.extent.width = static_cast<uint32_t>(m_wd->Width);
    render_pass_begin_info.renderArea.extent.height = static_cast<uint32_t>(m_wd->Height);
    render_pass_begin_info.clearValueCount = 1;
    render_pass_begin_info.pClearValues = &m_wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    // Record dear ImGui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);

    std::vector<VkSemaphore> wait_semaphores = {image_acquired_semaphore};
    std::vector<VkPipelineStageFlags> wait_stages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    if (wait_semaphore != nullptr)
    {
        const auto& native_semaphore = std::dynamic_pointer_cast<Vulkan::VulkanSemaphore>(wait_semaphore);
        wait_semaphores.push_back(native_semaphore->handle());

        wait_stages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    }

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &fd->CommandBuffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_complete_semaphore;

    VK_CHECK(vkEndCommandBuffer(fd->CommandBuffer));
    VK_CHECK(vkQueueSubmit(Vulkan::VulkanContext.device->get_graphics_queue()->handle(), 1, &submit_info, fd->Fence));
}

void ImGuiVulkanImpl::present_frame()
{
    if (m_rebuild_swapchain)
        return;

    const VkSemaphore render_complete_semaphore =
        m_wd->FrameSemaphores[static_cast<int32_t>(m_wd->SemaphoreIndex)].RenderCompleteSemaphore;

    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &m_wd->Swapchain;
    info.pImageIndices = &m_wd->FrameIndex;

    VkResult err = vkQueuePresentKHR(Vulkan::VulkanContext.device->get_graphics_queue()->handle(), &info);

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

    m_wd->SemaphoreIndex = (m_wd->SemaphoreIndex + 1) % m_wd->ImageCount;
}

ImTextureID ImGuiVulkanImpl::add_texture(const ImageResourceView& view)
{
    const Vulkan::VulkanImageResourceView& native_view = dynamic_cast<const Vulkan::VulkanImageResourceView&>(view);
    const Vulkan::VulkanSamplerState& native_sampler = dynamic_cast<const Vulkan::VulkanSamplerState&>(*m_sampler);

    return (ImTextureID)ImGui_ImplVulkan_AddTexture(
        native_sampler.handle(), native_view.handle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
