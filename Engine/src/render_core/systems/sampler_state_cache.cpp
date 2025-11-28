#include "sampler_state_cache.h"

#include "render_core/rhi/sampler_state.h"
#include "sampler_state_cache.h"

namespace Mizu
{

std::shared_ptr<SamplerState> Mizu::SamplerStateCache::get_sampler_state(const SamplerStateDescription& options)
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
    std::hash<ImageFilter> filter_hasher;
    std::hash<ImageAddressMode> address_mode_hasher;
    std::hash<BorderColor> border_color_hasher;

    size_t h = 0;

    h ^= filter_hasher(options.minification_filter);
    h ^= filter_hasher(options.magnification_filter);

    h ^= address_mode_hasher(options.address_mode_u);
    h ^= address_mode_hasher(options.address_mode_v);
    h ^= address_mode_hasher(options.address_mode_w);

    h ^= border_color_hasher(options.border_color);

    return h;
}

} // namespace Mizu