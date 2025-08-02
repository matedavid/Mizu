#pragma once

#include <memory>
#include <string>

namespace Mizu
{

// Forward declarations
class Framebuffer;

class RenderPass
{
  public:
    struct Description
    {
        std::shared_ptr<Framebuffer> target_framebuffer;
    };

    virtual ~RenderPass() = default;

    [[nodiscard]] static std::shared_ptr<RenderPass> create(const Description& desc);

    [[nodiscard]] virtual std::shared_ptr<Framebuffer> get_framebuffer() const = 0;
};

} // namespace Mizu
