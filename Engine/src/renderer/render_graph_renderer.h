#pragma once

namespace Mizu
{

// Forward declarations
class Camera;
class Texture2D;
class RenderGraphBuilder;

class RenderGraphRenderer
{
  public:
    void build(RenderGraphBuilder& builder, const Camera& camera, const Texture2D& output);
};

} // namespace Mizu
