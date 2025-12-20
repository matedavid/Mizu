#include "render_core/rhi/resource_view.h"

namespace Mizu
{

ImageResourceViewRange ImageResourceViewRange::from_mips_layers(
    uint32_t mip_base,
    uint32_t mip_count,
    uint32_t layer_base,
    uint32_t layer_count)
{
    ImageResourceViewRange range{};
    range.m_mip_base = mip_base;
    range.m_mip_count = mip_count;
    range.m_layer_base = layer_base;
    range.m_layer_count = layer_count;

    return range;
}

ImageResourceViewRange ImageResourceViewRange::from_mips(uint32_t mip_base, uint32_t mip_count)
{
    ImageResourceViewRange range{};
    range.m_mip_base = mip_base;
    range.m_mip_count = mip_count;

    return range;
}

ImageResourceViewRange ImageResourceViewRange::from_layers(uint32_t layer_base, uint32_t layer_count)
{
    ImageResourceViewRange range{};
    range.m_layer_base = layer_base;
    range.m_layer_count = layer_count;

    return range;
}

} // namespace Mizu
