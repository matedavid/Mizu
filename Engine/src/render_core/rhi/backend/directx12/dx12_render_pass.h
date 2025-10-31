#pragma once

#include "render_core/rhi/render_pass.h"

namespace Mizu::Dx12
{

class Dx12RenderPass : public RenderPass
{
  public:
    Dx12RenderPass(Description desc);

    std::shared_ptr<Framebuffer> get_framebuffer() const override { return m_description.target_framebuffer; }

  private:
    Description m_description;
};

} // namespace Mizu::Dx12