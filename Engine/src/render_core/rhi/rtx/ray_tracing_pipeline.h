#pragma once

#include <memory>
#include <vector>

namespace Mizu
{

// Forward declarations
class Shader;

class RayTracingPipeline
{
  public:
    struct Description
    {
        std::shared_ptr<Shader> raygen_shader;
        std::vector<std::shared_ptr<Shader>> miss_shaders;
        std::vector<std::shared_ptr<Shader>> closest_hit_shaders;

        uint32_t max_ray_recursion_depth = 1;
    };

    virtual ~RayTracingPipeline() = default;

    static std::shared_ptr<RayTracingPipeline> create(const Description& desc);
};

} // namespace Mizu