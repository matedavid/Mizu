#pragma once

#include <memory>

namespace Mizu {

// Forward declarations
class ComputeShader;

class ComputePipeline {
  public:
    struct Description {
        std::shared_ptr<ComputeShader> shader;
    };

    virtual ~ComputePipeline() = default;

    [[nodiscard]] static std::shared_ptr<ComputePipeline> create(const Description& desc);
};

} // namespace Mizu