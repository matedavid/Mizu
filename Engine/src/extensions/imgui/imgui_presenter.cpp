#include "imgui_presenter.h"

#include <imgui_internal.h>

#include "application/application.h"
#include "application/window.h"

#include "extensions/imgui/imgui_native_impl.h"
#include "extensions/imgui/imgui_vulkan_impl.h"

#include "render_core/rhi/renderer.h"
#include "render_core/rhi/swapchain.h"

#include "utility/assert.h"

namespace Mizu
{

ImGuiPresenter::ImGuiPresenter(std::shared_ptr<Window> window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

    ImGui::StyleColorsDark();

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        m_native_impl = std::make_unique<ImGuiVulkanImpl>(std::move(window));
        break;
    default:
        MIZU_UNREACHABLE("GraphicsAPI not implemented");
    }
}

ImGuiPresenter::~ImGuiPresenter()
{
    m_native_impl.reset();
    ImGui::DestroyContext();
}

void ImGuiPresenter::acquire_next_image(std::shared_ptr<Semaphore> signal_semaphore,
                                        std::shared_ptr<Fence> signal_fence)
{
    m_native_impl->new_frame(signal_semaphore, signal_fence);
    ImGui::NewFrame();
}

void ImGuiPresenter::render_imgui_and_present(const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores)
{
    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();

    const bool is_minimized = draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f;
    if (!is_minimized)
    {
        m_native_impl->render_frame(wait_semaphores);
        m_native_impl->present_frame();
    }
}

void ImGuiPresenter::set_background_texture(ImTextureID texture) const
{
    // TODO: Doesn't make much sense that we pass the window to the constructor and then access it this way...
    const uint32_t width = Application::instance()->get_window()->get_width();
    const uint32_t height = Application::instance()->get_window()->get_height();

    const float fwidth = static_cast<float>(width);
    const float fheight = static_cast<float>(height);

    ImGui::GetBackgroundDrawList()->AddImage(
        texture, ImVec2(0, 0), ImVec2(fwidth, fheight), ImVec2(0, 0), ImVec2(1, 1));
}

ImTextureID ImGuiPresenter::add_texture(const ImageResourceView& view) const
{
    return m_native_impl->add_texture(view);
}

void ImGuiPresenter::remove_texture(ImTextureID id) const
{
    m_native_impl->remove_texture(id);
}

} // namespace Mizu
