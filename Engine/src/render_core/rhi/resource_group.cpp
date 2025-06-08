#include "resource_group.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/resource_view.h"
#include "render_core/rhi/rtx/acceleration_structure.h"
#include "render_core/rhi/sampler_state.h"

#include "render_core/rhi/backend/opengl/opengl_resource_group.h"
#include "render_core/rhi/backend/vulkan/vulkan_resource_group.h"

namespace Mizu
{

//
// ResourceGroupLayout
//

size_t ResourceGroupLayout::get_hash() const
{
    std::hash<uint32_t> uint32_hasher;
    std::hash<ShaderType> shader_type_hasher;

    size_t hash = 0;

    for (const LayoutResource& resource : m_resources)
    {
        hash ^= uint32_hasher(resource.binding);
        hash ^= shader_type_hasher(resource.stage);

        if (resource.is_type<ImageResourceView>())
        {
            const auto& value = resource.as_type<ImageResourceView>();
            hash ^= std::hash<ImageResourceView*>()(value.get());
        }
        else if (resource.is_type<BufferResource>())
        {
            const auto& value = resource.as_type<BufferResource>();
            hash ^= std::hash<BufferResource*>()(value.get());
        }
        else if (resource.is_type<SamplerState>())
        {
            const auto& value = resource.as_type<SamplerState>();
            hash ^= std::hash<SamplerState*>()(value.get());
        }
        else if (resource.is_type<TopLevelAccelerationStructure>())
        {
            const auto& value = resource.as_type<TopLevelAccelerationStructure>();
            hash ^= std::hash<TopLevelAccelerationStructure*>()(value.get());
        }
        else
        {
            MIZU_UNREACHABLE("Resource type is not implemented");
        }
    }

    return hash;
}

//
// ResourceGroup
//

std::shared_ptr<ResourceGroup> ResourceGroup::create(const ResourceGroupLayout& layout)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanResourceGroup>(layout);
    case GraphicsAPI::OpenGL:
        MIZU_UNREACHABLE("Not implemented");
        return nullptr;
    }
}

} // namespace Mizu
