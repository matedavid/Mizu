#pragma once

#include <memory>

namespace Mizu
{

// Forward declarations
class Shader;

class RayTracingPipeline
{
  public:
    struct Description
    {
        std::shared_ptr<Shader> ray_generation_shader;
        std::shared_ptr<Shader> miss_shader;
        std::shared_ptr<Shader> closest_hit_shader;

        uint32_t max_ray_recursion_depth = 1;
    };

    virtual ~RayTracingPipeline() = default;

    static std::shared_ptr<RayTracingPipeline> create(const Description& desc);
};

} // namespace Mizu