#pragma once

#include "imgui_impl.h"

#include <vulkan/vulkan.h>

// Forward declarations
struct ImGui_ImplVulkanH_Window;

namespace Mizu
{

class ImGuiVulkanImpl : public INativeImGuiImpl
{
  public:
    ImGuiVulkanImpl(std::shared_ptr<Window> window);
    ~ImGuiVulkanImpl() override;

    void new_frame() override;
    void render_frame(ImDrawData* draw_data) override;
    void present_frame() override;

    [[nodiscard]] ImTextureID add_texture(const Texture2D& texture) override;
    void remove_texture(ImTextureID texture_id) override;

  private:
    std::shared_ptr<Window> m_window;
    ImGui_ImplVulkanH_Window* m_wd;

    VkDescriptorPool m_descriptor_pool;
    bool m_rebuild_swapchain = false;

    uint32_t m_min_image_count = 2;

    void create_resize_window();
};

} // namespace Mizu