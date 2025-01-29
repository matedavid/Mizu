#include "imgui_layer.h"

#include "imgui_impl.h"

#include "core/application.h"

namespace Mizu
{

ImGuiLayer::ImGuiLayer()
{
    ImGuiImpl::initialize(Application::instance()->get_window());
}

ImGuiLayer::~ImGuiLayer()
{
    ImGuiImpl::shutdown();
}

void ImGuiLayer::on_update(double ts)
{
    ImGuiImpl::new_frame();
    ImGui::NewFrame();

    on_update_impl(ts);

    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();

    const bool is_minimized = draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f;
    if (!is_minimized)
    {
        ImGuiImpl::render_frame(draw_data);
        ImGuiImpl::present_frame();
    }
}

} // namespace Mizu
