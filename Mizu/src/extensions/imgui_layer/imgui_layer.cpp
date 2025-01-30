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
}

} // namespace Mizu
