#pragma once

#include <glm/glm.hpp>

namespace Mizu
{

// Forward declaratoins
class Camera;
class Texture2D;
class Semaphore;

class ISceneRenderer
{
  public:
    virtual ~ISceneRenderer() = default;

    virtual void render(const Camera& camera) = 0;
    virtual void resize(uint32_t width, uint32_t height) = 0;

    virtual std::shared_ptr<Texture2D> get_result_texture() const = 0;
    virtual std::shared_ptr<Semaphore> get_render_semaphore() const = 0;
};

} // namespace Mizu