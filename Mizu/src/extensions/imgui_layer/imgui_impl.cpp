#include "imgui_impl.h"

#include <imgui_internal.h>

#include "imgui_vulkan_impl.h"

#include "core/application.h"
#include "core/window.h"

#include "render_core/resources/texture.h"
#include "render_core/rhi/renderer.h"

#include "utility/assert.h"

namespace Mizu
{

std::unique_ptr<INativeImGuiImpl> ImGuiImpl::s_native_impl = nullptr;
uint32_t ImGuiImpl::m_num_references = 0;

void ImGuiImpl::initialize(std::shared_ptr<Window> window)
{
    m_num_references += 1;

    if (s_native_impl != nullptr)
    {
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        s_native_impl = std::make_unique<ImGuiVulkanImpl>(window);
        break;
    default:
        MIZU_UNREACHABLE("GraphicsAPI not implemented");
    }
}

void ImGuiImpl::shutdown()
{
    m_num_references -= 1;

    if (m_num_references == 0)
    {
        s_native_impl.reset();
        ImGui::DestroyContext();
    }
}

void ImGuiImpl::new_frame()
{
    s_native_impl->new_frame();
}

void ImGuiImpl::render_frame(ImDrawData* draw_data, std::shared_ptr<Semaphore> wait_semaphore)
{
    s_native_impl->render_frame(draw_data, wait_semaphore);
}

void ImGuiImpl::present_frame()
{
    s_native_impl->present_frame();
}

ImTextureID ImGuiImpl::add_texture(const ImageResourceView& view)
{
    return s_native_impl->add_texture(view);
}

void ImGuiImpl::remove_texture(ImTextureID texture_id)
{
    s_native_impl->remove_texture(texture_id);
}

void ImGuiImpl::set_background_image(ImTextureID texture)
{
    const uint32_t width = Application::instance()->get_window()->get_width();
    const uint32_t height = Application::instance()->get_window()->get_height();

    const float fwidth = static_cast<float>(width);
    const float fheight = static_cast<float>(height);

    ImGui::GetBackgroundDrawList()->AddImage(
        texture, ImVec2(0, 0), ImVec2(fwidth, fheight), ImVec2(0, 0), ImVec2(1, 1));
}

void ImGuiImpl::present()
{
    present(nullptr);
}

void ImGuiImpl::present(std::shared_ptr<Semaphore> wait_semaphore)
{
    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();

    const bool is_minimized = draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f;
    if (!is_minimized)
    {
        ImGuiImpl::render_frame(draw_data, wait_semaphore);
        ImGuiImpl::present_frame();
    }
}

} // namespace Mizu
