#pragma once

#include <memory>
#include <unordered_map>

#include "render_core/rhi/sampler_state.h"

#include "mizu_render_module.h"

namespace Mizu
{

class MIZU_RENDER_API SamplerStateCache
{
  public:
    SamplerStateCache() = default;

    static SamplerStateCache& get();
    void reset();

    std::shared_ptr<SamplerState> get_sampler_state(const SamplerStateDescription& options);

  private:
    std::unordered_map<size_t, std::shared_ptr<SamplerState>> m_cache;

    size_t hash(const SamplerStateDescription& options) const;
};

MIZU_RENDER_API std::shared_ptr<SamplerState> get_sampler_state(const SamplerStateDescription& desc);

} // namespace Mizu