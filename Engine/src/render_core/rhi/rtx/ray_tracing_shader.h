#pragma once

#include "render_core/rhi/shader.h"

namespace Mizu
{

enum class RayTracingShaderStage
{
    Raygen,
    AnyHit,
    ClosestHit,
    Miss,
    Intersection,
};

struct RayTracingShaderStageInfo
{
    RayTracingShaderStage stage;
};

class RayTracingShader
{
  public:
    static std::shared_ptr<RayTracingShader> create(const RayTracingShaderStageInfo& info);
};

} // namespace Mizu