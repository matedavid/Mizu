#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <Mizu/Mizu.h>

#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_device.h"
#include "backend/vulkan/vulkan_image.h"
#include "backend/vulkan/vulkan_instance.h"
#include "backend/vulkan/vulkan_queue.h"
#include "backend/vulkan/vulkan_texture.h"

// Data
static VkAllocationCallbacks* g_Allocator = nullptr;
static VkInstance g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice g_Device = VK_NULL_HANDLE;
static uint32_t g_QueueFamily = (uint32_t)-1;
static VkQueue g_Queue = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
static VkPipelineCache g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;

static ImGui_ImplVulkanH_Window g_MainWindowData;
static int g_MinImageCount = 2;
static bool g_SwapChainRebuild = false;

// Mizu Rendering info
static std::shared_ptr<Mizu::Texture2D> g_ColorTexture;
static std::shared_ptr<Mizu::Framebuffer> g_Framebuffer;

static std::shared_ptr<Mizu::RenderPass> g_RenderPass;
static std::shared_ptr<Mizu::GraphicsPipeline> g_Pipeline;

static std::shared_ptr<Mizu::RenderCommandBuffer> g_CommandBuffer;

struct Vertex {
    glm::vec3 position;
};
static std::shared_ptr<Mizu::VertexBuffer> g_VertexBuffer;
static std::shared_ptr<Mizu::IndexBuffer> g_IndexBuffer;

static std::shared_ptr<Mizu::Fence> g_FlightFence;

// ImGui Image
static ImTextureID g_PresentTexture;

static void check_vk_result(VkResult err) {
    assert(err == VK_SUCCESS && "Vulkan call failed");
}

static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension) {
    for (const VkExtensionProperties& p : properties)
        if (strcmp(p.extensionName, extension) == 0)
            return true;
    return false;
}

static void SetupVulkan(ImVector<const char*> instance_extensions) {
    std::vector<std::string> extensions;
    for (size_t i = 0; i < instance_extensions.Size; ++i) {
        extensions.push_back(instance_extensions[i]);
    }

    Mizu::Configuration config{};
    config.graphics_api = Mizu::GraphicsAPI::Vulkan;
    config.backend_specific_config = Mizu::VulkanSpecificConfiguration{.instance_extensions = extensions};
    config.requirements = Mizu::Requirements{.graphics = true, .compute = false};

    Mizu::initialize(config);

    g_Instance = Mizu::Vulkan::VulkanContext.instance->handle();
    g_Device = Mizu::Vulkan::VulkanContext.device->handle();
    g_PhysicalDevice = Mizu::Vulkan::VulkanContext.device->physical_device();

    g_QueueFamily = Mizu::Vulkan::VulkanContext.device->get_graphics_queue()->family();
    g_Queue = Mizu::Vulkan::VulkanContext.device->get_graphics_queue()->handle();

    // Create Descriptor Pool
    {
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
        pool_info.maxSets = 20;
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        auto err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
        check_vk_result(err);
    }
}

static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height) {
    wd->Surface = surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, wd->Surface, &res);
    if (res != VK_TRUE) {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = {
        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM};
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice,
                                                              wd->Surface,
                                                              requestSurfaceImageFormat,
                                                              (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
                                                              requestSurfaceColorSpace);

    // Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
    VkPresentModeKHR present_modes[] = {
        VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR};
#else
    VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};
#endif
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
        g_PhysicalDevice, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
    // printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(g_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(
        g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
}

static void CleanupVulkan() {
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
    Mizu::shutdown();
}

static void CleanupVulkanWindow() {
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data) {
    VkResult err;

    VkSemaphore image_acquired_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    err = vkAcquireNextImageKHR(
        g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        g_SwapChainRebuild = true;
        return;
    }
    check_vk_result(err);

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(
            g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX); // wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(g_Device, 1, &fd->Fence);
        check_vk_result(err);
    }
    {
        err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
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
        check_vk_result(err);
        err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
        check_vk_result(err);
    }
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd) {
    if (g_SwapChainRebuild)
        return;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(g_Queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
        g_SwapChainRebuild = true;
        return;
    }
    check_vk_result(err);
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount; // Now we can use the next set of semaphores
}

static ImTextureID AddTexture(const std::shared_ptr<Mizu::Texture2D>& texture) {
    const auto& native_texture = std::dynamic_pointer_cast<Mizu::Vulkan::VulkanTexture2D>(texture);
    const auto& native_image = std::dynamic_pointer_cast<Mizu::Vulkan::VulkanImage>(native_texture->get_image());

    return ImGui_ImplVulkan_AddTexture(
        native_texture->get_sampler(), native_image->get_image_view(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

static void CreateRenderingInfo(uint32_t width, uint32_t height) {
    g_CommandBuffer = Mizu::RenderCommandBuffer::create();

    Mizu::ImageDescription color_desc{};
    color_desc.width = width;
    color_desc.height = height;
    color_desc.format = Mizu::ImageFormat::BGRA8_SRGB;
    color_desc.attachment = true;

    g_ColorTexture = Mizu::Texture2D::create(color_desc);

    Mizu::Framebuffer::Attachment color_attachment{};
    color_attachment.image = g_ColorTexture;
    color_attachment.load_operation = Mizu::LoadOperation::Clear;
    color_attachment.store_operation = Mizu::StoreOperation::Store;
    color_attachment.clear_value = glm::vec3(0.0f);

    Mizu::Framebuffer::Description framebuffer_desc{};
    framebuffer_desc.width = width;
    framebuffer_desc.height = height;
    framebuffer_desc.attachments = {color_attachment};

    g_Framebuffer = Mizu::Framebuffer::create(framebuffer_desc);

    const auto shader = Mizu::Shader::create("vertex.spv", "fragment.spv");

    Mizu::GraphicsPipeline::Description pipeline_desc{};
    pipeline_desc.shader = shader;
    pipeline_desc.target_framebuffer = g_Framebuffer;
    pipeline_desc.depth_stencil = Mizu::DepthStencilState{
        .depth_test = false,
        .depth_write = false,
    };

    g_Pipeline = Mizu::GraphicsPipeline::create(pipeline_desc);

    Mizu::RenderPass::Description render_pass_desc{};
    render_pass_desc.debug_name = "PruebasRenderPass";
    render_pass_desc.target_framebuffer = g_Framebuffer;

    g_RenderPass = Mizu::RenderPass::create(render_pass_desc);

    const std::vector<Vertex> vertex_data = {
        Vertex{.position = glm::vec3{-0.5, 0.5f, 0.0}},
        Vertex{.position = glm::vec3{0.5, 0.5f, 0.0}},
        Vertex{.position = glm::vec3{0.0, -0.5f, 0.0}},
    };
    g_VertexBuffer = Mizu::VertexBuffer::create(vertex_data);
    g_IndexBuffer = Mizu::IndexBuffer::create(std::vector<uint32_t>{0, 1, 2});

    g_FlightFence = Mizu::Fence::create();

    g_PresentTexture = AddTexture(g_ColorTexture);
}

static void DestroyRenderingInfo() {
    g_ColorTexture = nullptr;
    g_Framebuffer = nullptr;

    g_RenderPass = nullptr;
    g_Pipeline = nullptr;

    g_CommandBuffer = nullptr;

    g_VertexBuffer = nullptr;
    g_IndexBuffer = nullptr;

    g_FlightFence = nullptr;
}

static void MizuRenderLogic() {
    g_FlightFence->wait_for();

    g_CommandBuffer->begin();
    {
        g_RenderPass->begin(g_CommandBuffer);

        g_Pipeline->bind(g_CommandBuffer);
        Mizu::draw_indexed(g_CommandBuffer, g_VertexBuffer, g_IndexBuffer);

        g_RenderPass->end(g_CommandBuffer);
    }
    g_CommandBuffer->end();

    Mizu::CommandBufferSubmitInfo submit_info{};
    submit_info.signal_fence = g_FlightFence;

    g_CommandBuffer->submit(submit_info);
}

// Main code
int main() {
    if (!glfwInit())
        return 1;

    // Create window with Vulkan context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+Vulkan example", nullptr, nullptr);
    if (!glfwVulkanSupported()) {
        printf("GLFW: Vulkan Not Supported\n");
        return 1;
    }

    ImVector<const char*> extensions;
    uint32_t extensions_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++)
        extensions.push_back(glfw_extensions[i]);
    SetupVulkan(extensions);

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err = glfwCreateWindowSurface(g_Instance, window, g_Allocator, &surface);
    check_vk_result(err);

    // Create Framebuffers
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    SetupVulkanWindow(wd, surface, w, h);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.RenderPass = wd->RenderPass;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Create Mizu rendering info
    CreateRenderingInfo(w, h);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Resize swap chain?
        if (g_SwapChainRebuild) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            if (width > 0 && height > 0) {
                ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance,
                                                       g_PhysicalDevice,
                                                       g_Device,
                                                       &g_MainWindowData,
                                                       g_QueueFamily,
                                                       g_Allocator,
                                                       width,
                                                       height,
                                                       g_MinImageCount);
                g_MainWindowData.FrameIndex = 0;
                g_SwapChainRebuild = false;

                w = width;
                h = height;

                ImGui_ImplVulkan_RemoveTexture(static_cast<VkDescriptorSet>(g_PresentTexture));
                CreateRenderingInfo(w, h);
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        // Draw
        MizuRenderLogic();
        ImGui::GetBackgroundDrawList()->AddImage(g_PresentTexture, ImVec2(0, 0), ImVec2(w, h));

        // Rendering
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized) {
            wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            wd->ClearValue.color.float32[3] = clear_color.w;
            FrameRender(wd, draw_data);
            FramePresent(wd);
        }
    }

    // Cleanup
    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    DestroyRenderingInfo();

    CleanupVulkanWindow();
    CleanupVulkan();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}