#include "render_core/rhi/resource_group.h"

namespace Mizu
{

ResourceGroupBuilder& ResourceGroupBuilder::add_resource(ResourceGroupItem item)
{
    m_resources.push_back(item);

    return *this;
}

size_t ResourceGroupBuilder::get_hash() const
{
    size_t hash = 0;

    for (const ResourceGroupItem& item : m_resources)
    {
        hash ^= item.hash();
    }

    return hash;
}

} // namespace Mizu
