#pragma once

namespace Mizu
{

// Forward declarations
struct CameraDynamicState;
class Texture2D;
class RenderGraphBuilder;

class RenderGraphRenderer
{
  public:
    void build(RenderGraphBuilder& builder, const CameraDynamicState& camera, const Texture2D& output);
};

} // namespace Mizu
