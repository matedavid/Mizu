#pragma once

#include <memory>
#include <vulkan/vulkan.h>

#include "extensions/imgui/imgui_native_impl.h"

// Forward declarations
struct ImGui_ImplVulkanH_Window;

namespace Mizu
{

// Forward declarations
class Window;
class SamplerState;

class ImGuiVulkanImpl : public IImGuiNativeImpl
{
  public:
    ImGuiVulkanImpl(std::shared_ptr<Window> window);
    ~ImGuiVulkanImpl() override;

    void new_frame(std::shared_ptr<Semaphore> signal_semaphore, std::shared_ptr<Fence> signal_fence) override;
    void render_frame(const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores) const override;
    void present_frame() override;

    ImTextureID add_texture(const ImageResourceView& view) const override;
    void remove_texture(ImTextureID id) const override;

  private:
    std::shared_ptr<Window> m_window;
    std::unique_ptr<ImGui_ImplVulkanH_Window> m_wnd;
    VkDescriptorPool m_descriptor_pool;

    std::shared_ptr<SamplerState> m_sampler;

    bool m_rebuild_swapchain = false;

    void create_resize_window();
};

} // namespace Mizu
