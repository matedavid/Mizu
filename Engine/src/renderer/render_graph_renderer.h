#pragma once

namespace Mizu
{

// Forward declarations
class Camera;
class RenderGraphBuilder;
class Texture2D;

class RenderGraphRenderer
{
  public:
    void build(RenderGraphBuilder& builder, const Camera& camera, const Texture2D& output);
};

} // namespace Mizu
