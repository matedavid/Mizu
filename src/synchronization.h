#pragma once

#include <memory>

namespace Mizu {

class Fence {
  public:
    virtual ~Fence() = default;

    [[nodiscard]] static std::shared_ptr<Fence> create();

    virtual void wait_for() const = 0;
};

class Semaphore {
  public:
    virtual ~Semaphore() = default;

    [[nodiscard]] static std::shared_ptr<Semaphore> create();
};

} // namespace Mizu
