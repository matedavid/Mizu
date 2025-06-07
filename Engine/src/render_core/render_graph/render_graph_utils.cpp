#include "render_graph_utils.h"

#include "render_core/render_graph/render_graph_resources.h"

#include "render_core/rhi/resource_group.h"

namespace Mizu
{

void bind_resource_group(RenderCommandBuffer& command,
                         const RGPassResources& resources,
                         const RGResourceGroupRef& ref,
                         uint32_t set)
{
    const std::shared_ptr<ResourceGroup>& resource_group = resources.get_resource_group(ref);
    MIZU_ASSERT(resource_group != nullptr, "Invalid ResourceGroup");

    command.bind_resource_group(resource_group, set);
}

} // namespace Mizu::RGFunctions