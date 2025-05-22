#pragma once

#include <memory>

namespace Mizu
{

// Forward declarations
class RayTracingShader;

class RayTracingPipeline
{
  public:
    struct Description
    {
        std::shared_ptr<RayTracingShader> ray_generation_shader;
        std::shared_ptr<RayTracingShader> miss_shader;
        std::shared_ptr<RayTracingShader> closest_hit_shader;

        uint32_t max_ray_recursion_depth = 1;
    };

    virtual ~RayTracingPipeline() = default;

    static std::shared_ptr<RayTracingPipeline> create(const Description& desc);
};

} // namespace Mizu