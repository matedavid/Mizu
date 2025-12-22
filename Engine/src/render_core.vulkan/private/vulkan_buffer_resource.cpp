#include "vulkan_buffer_resource.h"

#include <cstring>

#include "base/debug/assert.h"

#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_device_memory_allocator.h"
#include "vulkan_resource_view.h"

namespace Mizu::Vulkan
{

VulkanBufferResource::VulkanBufferResource(const BufferDescription& desc) : m_description(desc)
{
    VkBufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = m_description.size;
    create_info.usage = get_vulkan_usage(m_description.usage);
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    MIZU_ASSERT(create_info.usage != 0, "Failed to create buffer '{}', no usage was specified", m_description.name);

    VK_CHECK(vkCreateBuffer(VulkanContext.device->handle(), &create_info, nullptr, &m_handle));

    if (!m_description.is_virtual)
    {
        m_allocation_info = VulkanContext.default_device_allocator->allocate_buffer_resource(*this);
    }

    if (!m_description.name.empty())
    {
        VK_DEBUG_SET_OBJECT_NAME(m_handle, m_description.name);
    }
}

VulkanBufferResource::~VulkanBufferResource()
{
    for (const ResourceView& view : m_resource_views)
    {
        if (view.internal == nullptr)
            continue;

        const VulkanBufferResourceView* internal = reinterpret_cast<const VulkanBufferResourceView*>(view.internal);
        delete internal;
    }

    if (!m_description.is_virtual)
    {
        VulkanContext.default_device_allocator->release(m_allocation_info.id);
    }

    vkDestroyBuffer(VulkanContext.device->handle(), m_handle, nullptr);
}

ResourceView VulkanBufferResource::as_srv()
{
    return get_or_create_resource_view(ResourceViewType::ShaderResourceView);
}

ResourceView VulkanBufferResource::as_uav()
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::UnorderedAccess,
        "Trying to create uav for buffer '{}' that was not created with UnorderedAccess usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::UnorderedAccessView);
}

ResourceView VulkanBufferResource::as_cbv()
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::ConstantBuffer,
        "Trying to create cbv for buffer '{}' that was not created with ConstantBuffer usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::ConstantBufferView);
}

ResourceView VulkanBufferResource::get_or_create_resource_view(ResourceViewType type)
{
    for (const ResourceView& view : m_resource_views)
    {
        if (view.internal == nullptr || view.view_type != type)
            continue;

        return view;
    }

    // Create new view
    VulkanBufferResourceView* internal = new VulkanBufferResourceView{};
    // TODO: Should enable specifying offset and size for the buffer view
    internal->offset = 0;
    internal->size = m_description.size;
    internal->handle = m_handle;

    ResourceView view{};
    view.view_type = type;
    view.internal = internal;

    m_resource_views.push_back(view);

    return view;
}

MemoryRequirements VulkanBufferResource::get_memory_requirements() const
{
    VkBufferMemoryRequirementsInfo2 vk_reqs_info2{};
    vk_reqs_info2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
    vk_reqs_info2.buffer = m_handle;

    VkMemoryDedicatedRequirements dedicated_reqs{};
    dedicated_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;

    VkMemoryRequirements2 vk_reqs2{};
    vk_reqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vk_reqs2.pNext = &dedicated_reqs;

    vkGetBufferMemoryRequirements2(VulkanContext.device->handle(), &vk_reqs_info2, &vk_reqs2);

    MemoryRequirements reqs{};
    reqs.size = vk_reqs2.memoryRequirements.size;
    reqs.alignment = vk_reqs2.memoryRequirements.alignment;

    return reqs;
}

void VulkanBufferResource::set_data(const uint8_t* data, size_t size, size_t offset) const
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::HostVisible, "Can't map data that does not have the HostVisible usage");

    uint8_t* mapped = VulkanContext.default_device_allocator->get_mapped_memory(m_allocation_info.id);
    MIZU_ASSERT(mapped != nullptr, "Memory is not mapped");

    memcpy(mapped + m_allocation_info.offset + offset, data, size);
}

VkBufferUsageFlags VulkanBufferResource::get_vulkan_usage(BufferUsageBits usage)
{
    VkBufferUsageFlags vulkan_usage = 0;

    if (usage & BufferUsageBits::VertexBuffer)
        vulkan_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    if (usage & BufferUsageBits::IndexBuffer)
        vulkan_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (usage & BufferUsageBits::ConstantBuffer)
        vulkan_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (usage & BufferUsageBits::UnorderedAccess)
        vulkan_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    const VkBufferUsageFlags type_related_flags = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                                  | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
                                                  | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    // In Vulkan, there does not exist a concept of a ReadOnly StructuredBuffer, they are treated as storage buffers.
    // Therefore, if we have a buffer that does not have any other type-related usage flag, we supposed it's a ReadOnly
    // StructuredBuffer and we set the VK_BUFFER_USAGE_STORAGE_BUFFER_BIT usage flag.
    if ((vulkan_usage & type_related_flags) == 0)
        vulkan_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (usage & BufferUsageBits::TransferSrc)
        vulkan_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (usage & BufferUsageBits::TransferDst)
        vulkan_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (usage & BufferUsageBits::RtxAccelerationStructureStorage)
        vulkan_usage |=
            (VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    if (usage & BufferUsageBits::RtxAccelerationStructureInputReadOnly)
        vulkan_usage |=
            (VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
             | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    if (usage & BufferUsageBits::RtxShaderBindingTable)
        vulkan_usage |= (VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    return vulkan_usage;
}

} // namespace Mizu::Vulkan