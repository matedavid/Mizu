#pragma once

#include "render_core/rhi/renderer.h"

namespace Mizu::Dx12
{

class Dx12Backend : public IBackend
{
  public:
    ~Dx12Backend() override;

    bool initialize(const RendererConfiguration& config) override;

    void wait_idle() const override;

    RendererCapabilities get_capabilities() const override;
};

} // namespace Mizu::Dx12