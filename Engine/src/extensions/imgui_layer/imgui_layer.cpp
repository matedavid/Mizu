#include "imgui_layer.h"

#include "core/application.h"
#include "extensions/imgui_layer/imgui_presenter.h"

namespace Mizu
{

ImGuiLayer::ImGuiLayer()
{
    // ImGuiImpl::initialize(Application::instance()->get_window());
}

ImGuiLayer::~ImGuiLayer()
{
    // ImGuiImpl::shutdown();
}

void ImGuiLayer::on_update(double ts)
{
    (void)ts;

    /*
    ImGuiImpl::new_frame();
    ImGui::NewFrame();

    on_update_impl(ts);
    */
}

} // namespace Mizu
