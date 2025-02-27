#pragma once

#include "imgui_impl.h"

#include <vulkan/vulkan.h>

// Forward declarations
struct ImGui_ImplVulkanH_Window;

namespace Mizu
{

// Forward declarations
class SamplerState;

class ImGuiVulkanImpl : public INativeImGuiImpl
{
  public:
    ImGuiVulkanImpl(std::shared_ptr<Window> window);
    ~ImGuiVulkanImpl() override;

    void new_frame() override;
    void render_frame(ImDrawData* draw_data, std::shared_ptr<Semaphore> wait_semaphore) override;
    void present_frame() override;

    [[nodiscard]] ImTextureID add_texture(const ImageResourceView& view) override;
    void remove_texture(ImTextureID texture_id) override;

  private:
    std::shared_ptr<Window> m_window;
    ImGui_ImplVulkanH_Window* m_wd;

    // TODO: Should probably be on the top level, as it's api agnostic
    std::shared_ptr<SamplerState> m_sampler;

    VkDescriptorPool m_descriptor_pool;
    bool m_rebuild_swapchain = false;

    uint32_t m_min_image_count = 2;

    void create_resize_window();
};

} // namespace Mizu