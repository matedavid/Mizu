#pragma once

#include <cstdint>

namespace Mizu
{

class Fence
{
  public:
    virtual ~Fence() = default;

    virtual uint64_t get_completed_value() const = 0;
    virtual void wait(uint64_t value) const = 0;
    virtual void signal(uint64_t value) = 0;
};

struct SyncPoint
{
    Fence* fence = nullptr;
    uint64_t value = 0;

    bool is_valid() const { return fence != nullptr; }
    bool is_complete() const { return fence == nullptr || fence->get_completed_value() >= value; }

    void wait() const
    {
        if (fence != nullptr)
            fence->wait(value);
    }
};

} // namespace Mizu
