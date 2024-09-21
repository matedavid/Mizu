#pragma once

#include <memory>

namespace Mizu {

class ComputePipeline {
  public:
    struct Description {};

    [[nodiscard]] static std::shared_ptr<ComputePipeline> create(const Description& desc);
};

} // namespace Mizu