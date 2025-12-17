#include "render_core/rhi/resource_group.h"

#include "backend/dx12/dx12_resource_group.h"
#include "backend/vulkan/vulkan_resource_group.h"
#include "render_core/rhi/acceleration_structure.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/renderer.h"
#include "render_core/rhi/resource_view.h"
#include "render_core/rhi/sampler_state.h"

namespace Mizu
{

//
// ResourceGroupLayout
//

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

//
// ResourceGroup
//

std::shared_ptr<ResourceGroup> ResourceGroup::create(const ResourceGroupBuilder& builder)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12ResourceGroup>(builder);
    case GraphicsApi::Vulkan:
        return std::make_shared<Vulkan::VulkanResourceGroup>(builder);
    }
}

} // namespace Mizu
