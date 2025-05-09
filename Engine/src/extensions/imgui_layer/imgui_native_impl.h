#pragma once

#include <imgui.h>
#include <memory>
#include <vector>

namespace Mizu
{

// Forward declarations
class Fence;
class Semaphore;
class ImageResourceView;

class IImGuiNativeImpl
{
  public:
    virtual ~IImGuiNativeImpl() = default;

    virtual void new_frame(std::shared_ptr<Semaphore> signal_semaphore, std::shared_ptr<Fence> signal_fence) = 0;
    virtual void render_frame(const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores) const = 0;
    virtual void present_frame() = 0;

    virtual ImTextureID add_texture(const ImageResourceView& view) const = 0;
    virtual void remove_texture(ImTextureID id) const = 0;
};

} // namespace Mizu