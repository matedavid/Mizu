#pragma once

#include <memory>

namespace Mizu {

// Forward declarations
class Camera;
class Texture2D;

class ISceneRenderer {
  public:
    virtual void render(const Camera& camera) = 0;
    virtual std::shared_ptr<Texture2D> output_texture() const = 0;
};

} // namespace Mizu