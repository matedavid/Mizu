#pragma once

#include <glm/glm.hpp>
#include <memory>

namespace Mizu
{

// Forward declarations
class Window;
class Texture2D;
class Semaphore;

class Presenter
{
  public:
    virtual ~Presenter() = default;

    [[nodiscard]] static std::shared_ptr<Presenter> create(const std::shared_ptr<Window>& window,
                                                           const std::shared_ptr<Texture2D>& texture);

    virtual void present() = 0;
    virtual void present(const std::shared_ptr<Semaphore>& wait_semaphore) = 0;

    virtual void texture_changed(std::shared_ptr<Texture2D> texture) = 0;

  protected:
    struct PresenterVertex
    {
        glm::vec3 position;
        glm::vec2 texture_coords;
    };
};

} // namespace Mizu
