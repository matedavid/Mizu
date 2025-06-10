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

ResourceGroupLayout& ResourceGroupLayout::add_resource(uint32_t binding,
                                                       std::shared_ptr<ImageResourceView> resource,
                                                       ShaderType stage,
                                                       ShaderImageProperty::Type type)
{
    LayoutResourceImageView value{};
    value.value = resource;
    value.type = type;

    LayoutResource item{};
    item.binding = binding;
    item.value = value;
    item.stage = stage;

    m_resources.push_back(item);

    return *this;
}

ResourceGroupLayout& ResourceGroupLayout::add_resource(uint32_t binding,
                                                       std::shared_ptr<BufferResource> resource,
                                                       ShaderType stage,
                                                       ShaderBufferProperty::Type type)
{
    LayoutResourceBuffer value{};
    value.value = resource;
    value.type = type;

    LayoutResource item{};
    item.binding = binding;
    item.value = value;
    item.stage = stage;

    m_resources.push_back(item);

    return *this;
}

ResourceGroupLayout& ResourceGroupLayout::add_resource(uint32_t binding,
                                                       std::shared_ptr<SamplerState> resource,
                                                       ShaderType stage)
{
    LayoutResourceSamplerState value{};
    value.value = resource;

    LayoutResource item{};
    item.binding = binding;
    item.value = value;
    item.stage = stage;

    m_resources.push_back(item);

    return *this;
}

ResourceGroupLayout& ResourceGroupLayout::add_resource(uint32_t binding,
                                                       std::shared_ptr<TopLevelAccelerationStructure> resource,
                                                       ShaderType stage)
{
    LayoutResourceTopLevelAccelerationStructure value{};
    value.value = resource;

    LayoutResource item{};
    item.binding = binding;
    item.value = value;
    item.stage = stage;

    m_resources.push_back(item);

    return *this;
}

size_t ResourceGroupLayout::get_hash() const
{
    std::hash<uint32_t> uint32_hasher;
    std::hash<ShaderType> shader_type_hasher;

    size_t hash = 0;

    for (const LayoutResource& resource : m_resources)
    {
        hash ^= uint32_hasher(resource.binding);
        hash ^= shader_type_hasher(resource.stage);

        if (resource.is_type<LayoutResourceImageView>())
        {
            const auto& value = resource.as_type<LayoutResourceImageView>();
            hash ^= std::hash<ImageResourceView*>()(value.value.get());
        }
        else if (resource.is_type<LayoutResourceBuffer>())
        {
            const auto& value = resource.as_type<LayoutResourceBuffer>();
            hash ^= std::hash<BufferResource*>()(value.value.get());
        }
        else if (resource.is_type<LayoutResourceSamplerState>())
        {
            const auto& value = resource.as_type<LayoutResourceSamplerState>();
            hash ^= std::hash<SamplerState*>()(value.value.get());
        }
        else if (resource.is_type<LayoutResourceTopLevelAccelerationStructure>())
        {
            const auto& value = resource.as_type<LayoutResourceTopLevelAccelerationStructure>();
            hash ^= std::hash<TopLevelAccelerationStructure*>()(value.value.get());
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
