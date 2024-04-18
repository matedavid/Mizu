#pragma once

#include <memory>
#include <optional>

namespace Mizu {

// Forward declarations
class Fence;
class Semaphore;

enum class CommandBufferType {
    Graphics,
    Compute,
    Transfer,
};

struct CommandBufferSubmitInfo {
    std::shared_ptr<Fence> signal_fence = nullptr;
    std::shared_ptr<Semaphore> wait_semaphore = nullptr;
    std::shared_ptr<Semaphore> signal_semaphore = nullptr;
};

class ICommandBuffer {
  public:
    virtual void begin() const = 0;
    virtual void end() const = 0;

    virtual void submit() const = 0;
    virtual void submit(const CommandBufferSubmitInfo& info) const = 0;
};

class RenderCommandBuffer : public ICommandBuffer {
  public:
    virtual ~RenderCommandBuffer() = default;

    [[nodiscard]] static std::shared_ptr<RenderCommandBuffer> create();
};

class ComputeCommandBuffer : public ICommandBuffer {
  public:
    virtual ~ComputeCommandBuffer() = default;

    [[nodiscard]] static std::shared_ptr<ComputeCommandBuffer> create();
};

} // namespace Mizu
