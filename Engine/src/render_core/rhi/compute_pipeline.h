#pragma once

#include <memory>

namespace Mizu
{

// Forward declarations
class Shader;

class ComputePipeline
{
  public:
    struct Description
    {
        std::shared_ptr<Shader> shader;
    };

    virtual ~ComputePipeline() = default;

    [[nodiscard]] static std::shared_ptr<ComputePipeline> create(const Description& desc);
};

} // namespace Mizu