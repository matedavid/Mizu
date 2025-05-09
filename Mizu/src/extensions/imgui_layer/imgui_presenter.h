#pragma once

#include <imgui.h>
#include <memory>
#include <vector>

#include "extensions/imgui_layer/imgui_native_impl.h"

namespace Mizu
{

// Forward declarations
class Window;
struct CommandBufferSubmitInfo;

class ImGuiPresenter
{
  public:
    ImGuiPresenter(std::shared_ptr<Window> window);
    ~ImGuiPresenter();

    void acquire_next_image(std::shared_ptr<Semaphore> signal_semaphore, std::shared_ptr<Fence> signal_fence);
    void render_imgui_and_present(const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores);

    void set_background_texture(ImTextureID texture) const;

    ImTextureID add_texture(const ImageResourceView& view) const;
    void remove_texture(ImTextureID id) const;

  private:
    std::unique_ptr<IImGuiNativeImpl> m_native_impl;
};

} // namespace Mizu
