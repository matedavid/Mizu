#pragma once

namespace Mizu
{

// Forward declarations
class Camera;
class Texture2D;
class Semaphore;
struct CommandBufferSubmitInfo;

class ISceneRenderer
{
  public:
    virtual ~ISceneRenderer() = default;

    virtual void render(const Camera& camera, const Texture2D& output, const CommandBufferSubmitInfo& submit_info) = 0;
};

} // namespace Mizu
