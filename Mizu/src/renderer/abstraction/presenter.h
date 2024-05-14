#pragma once

#include <cstdint>
#include <memory>

namespace Mizu {

// Forward declarations
class Window;
class Texture2D;

class Presenter {
  public:
    virtual ~Presenter() = default;

    [[nodiscard]] std::shared_ptr<Presenter> create(const std::shared_ptr<Window>& window,
                                                    const std::shared_ptr<Texture2D>& texture);

    virtual void present() = 0;
    virtual void window_resized(uint32_t width, uint32_t height) = 0;
};

} // namespace Mizu
