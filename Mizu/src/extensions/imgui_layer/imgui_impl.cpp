#include "imgui_impl.h"

#include <imgui_internal.h>

#include "imgui_vulkan_impl.h"

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

void ImGuiImpl::render_frame(ImDrawData* draw_data)
{
    s_native_impl->render_frame(draw_data);
}

void ImGuiImpl::present_frame()
{
    s_native_impl->present_frame();
}

ImTextureID ImGuiImpl::add_texture(const Texture2D& texture)
{
    return s_native_impl->add_texture(texture);
}

void ImGuiImpl::remove_texture(ImTextureID texture_id)
{
    s_native_impl->remove_texture(texture_id);
}

} // namespace Mizu
