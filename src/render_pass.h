#pragma once

#include <memory>

namespace Mizu {

// Forward declarations
class Framebuffer;
class ICommandBuffer;

class RenderPass {
  public:
    struct Description {
        std::string debug_name;
        std::shared_ptr<Framebuffer> target_framebuffer;
    };

    virtual ~RenderPass() = default;

    [[nodiscard]] static std::shared_ptr<RenderPass> create(const Description& desc);

    virtual void begin(const std::shared_ptr<ICommandBuffer>& command_buffer) const = 0;
    virtual void end(const std::shared_ptr<ICommandBuffer>& command_buffer) const = 0;
};

} // namespace Mizu