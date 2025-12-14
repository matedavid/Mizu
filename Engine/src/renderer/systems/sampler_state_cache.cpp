#include "sampler_state_cache.h"

#include "base/utils/hash.h"

#include "render_core/rhi/sampler_state.h"

namespace Mizu
{

SamplerStateCache& SamplerStateCache::get()
{
    static SamplerStateCache instance;
    return instance;
}

void SamplerStateCache::reset()
{
    m_cache.clear();
}

std::shared_ptr<SamplerState> SamplerStateCache::get_sampler_state(const SamplerStateDescription& options)
{
    const size_t h = hash(options);

    const auto it = m_cache.find(h);
    if (it != m_cache.end())
    {
        return it->second;
    }

    const auto sampler = SamplerState::create(options);
    return m_cache.insert({h, sampler}).first->second;
}

size_t SamplerStateCache::hash(const SamplerStateDescription& options) const
{
    return hash_compute(
        options.minification_filter,
        options.magnification_filter,
        options.address_mode_u,
        options.address_mode_v,
        options.address_mode_w,
        options.border_color);
}

std::shared_ptr<SamplerState> get_sampler_state(const SamplerStateDescription& desc)
{
    return SamplerStateCache::get().get_sampler_state(desc);
}

} // namespace Mizu