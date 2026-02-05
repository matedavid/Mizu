#include "vulkan_layout.h"

#include "base/debug/assert.h"

#include "vulkan_context.h"
#include "vulkan_shader.h"

namespace Mizu::Vulkan
{

static uint32_t get_binding_offset_for_descriptor_type(ShaderResourceType type)
{
    switch (type)
    {
    case ShaderResourceType::TextureSrv:
    case ShaderResourceType::StructuredBufferSrv:
    case ShaderResourceType::ByteAddressBufferSrv:
    case ShaderResourceType::AccelerationStructure:
        return VulkanContext.binding_offsets.srv_offset;
    case ShaderResourceType::TextureUav:
    case ShaderResourceType::StructuredBufferUav:
    case ShaderResourceType::ByteAddressBufferUav:
        return VulkanContext.binding_offsets.uav_offset;
    case ShaderResourceType::ConstantBuffer:
        return VulkanContext.binding_offsets.cbv_offset;
    case ShaderResourceType::SamplerState:
        return VulkanContext.binding_offsets.sampler_offset;
    case ShaderResourceType::PushConstant:
        MIZU_UNREACHABLE("PushConstant is invalid in this context");
        return 0;
    }
}

uint32_t get_binding_with_offset(uint32_t binding, ShaderResourceType type)
{
    return binding + get_binding_offset_for_descriptor_type(type);
}

//
// VulkanDescriptorSetLayoutCache
//

VulkanDescriptorSetLayoutCache::~VulkanDescriptorSetLayoutCache()
{
    for (const auto& [handle, layout] : m_cache)
    {
        vkDestroyDescriptorSetLayout(VulkanContext.device->handle(), layout, nullptr);
    }
}

DescriptorSetLayoutHandle VulkanDescriptorSetLayoutCache::create(const DescriptorSetLayoutDescription& desc)
{
    std::vector<VkDescriptorSetLayoutBinding> layout_bindings{};
    layout_bindings.reserve(desc.layout.size());

    std::vector<VkDescriptorBindingFlags> layout_binding_flags{};
    layout_binding_flags.reserve(desc.layout.size());

    size_t hash = 0;

    bool is_bindless = false;
    for (const DescriptorItem& item : desc.layout)
    {
        uint32_t descriptor_count = item.count;

        const auto is_array_resource_type = [](ShaderResourceType type) -> bool {
            return type == ShaderResourceType::TextureSrv;
        };

        if (item.count == BINDLESS_DESCRIPTOR_COUNT && is_array_resource_type(item.type))
        {
            // TODO: Currently hardcoding here, should take it from VkPhysicalDeviceDescriptorIndexingProperties
            descriptor_count = 4096;

            is_bindless = true;

            constexpr VkDescriptorBindingFlags bindless_flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
                                                                | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                                                                | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

            layout_binding_flags.push_back(bindless_flags);
        }
        else
        {
            layout_binding_flags.push_back(0);
        }

        VkDescriptorSetLayoutBinding layout_binding{};
        layout_binding.binding =
            desc.vulkan_apply_binding_offsets ? get_binding_with_offset(item.binding, item.type) : item.binding;
        layout_binding.descriptorType = VulkanShader::get_vulkan_descriptor_type(item.type);
        layout_binding.descriptorCount = descriptor_count;
        layout_binding.stageFlags = VulkanShader::get_vulkan_shader_stage_bits(item.stage);
        layout_binding.pImmutableSamplers = nullptr;

        layout_bindings.push_back(layout_binding);

        hash_combine(hash, item.hash());
    }

    // is_bindless -> layout_bindings.size() == 1
    MIZU_ASSERT(
        !is_bindless || layout_bindings.size() == 1,
        "Currently only supporting one descriptor binding for bindless layouts");

    const DescriptorSetLayoutHandle handle = DescriptorSetLayoutHandle{hash};

    if (contains(handle))
    {
        return handle;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info{};
    binding_flags_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags_create_info.pNext = nullptr;
    binding_flags_create_info.bindingCount = static_cast<uint32_t>(layout_binding_flags.size());
    binding_flags_create_info.pBindingFlags = layout_binding_flags.data();

    VkDescriptorSetLayoutCreateInfo layout_create_info{};
    layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_create_info.pNext = &binding_flags_create_info;
    layout_create_info.flags = is_bindless ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0;
    layout_create_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
    layout_create_info.pBindings = layout_bindings.data();

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(VulkanContext.device->handle(), &layout_create_info, nullptr, &layout));

    m_cache.insert({handle, layout});

    return handle;
}

VkDescriptorSetLayout VulkanDescriptorSetLayoutCache::get(DescriptorSetLayoutHandle handle) const
{
    MIZU_ASSERT(contains(handle), "VulkanDescriptorSetLayoutCache does not contain layout with handle {}", handle);
    return m_cache.find(handle)->second;
}

bool VulkanDescriptorSetLayoutCache::contains(DescriptorSetLayoutHandle handle) const
{
    return m_cache.find(handle) != m_cache.end();
}

//
// VulkanPipelineLayoutCache
//

VulkanPipelineLayoutCache::~VulkanPipelineLayoutCache()
{
    for (const auto& [handle, layout] : m_cache)
    {
        vkDestroyPipelineLayout(VulkanContext.device->handle(), layout, nullptr);
    }
}

PipelineLayoutHandle VulkanPipelineLayoutCache::create(const PipelineLayoutDescription& desc)
{
    size_t hash = 0;

    std::vector<VkDescriptorSetLayout> set_layouts;
    set_layouts.reserve(desc.set_layouts.size());

    std::vector<VkPushConstantRange> push_constant_ranges;
    push_constant_ranges.reserve(1);

    for (const DescriptorSetLayoutHandle& handle : desc.set_layouts)
    {
        if (handle == 0)
        {
            set_layouts.push_back(VK_NULL_HANDLE);
            continue;
        }

        MIZU_ASSERT(
            VulkanContext.descriptor_set_layout_cache->contains(handle),
            "Descriptor set layout with handle {} does not exist",
            handle);
        set_layouts.push_back(VulkanContext.descriptor_set_layout_cache->get(handle));

        hash_combine(hash, handle);
    }

    if (desc.push_constant.has_value())
    {
        const PushConstantItem& push_constant = *desc.push_constant;

        VkPushConstantRange range{};
        range.stageFlags = VulkanShader::get_vulkan_shader_stage_bits(push_constant.stage);
        range.offset = 0;
        range.size = push_constant.size;

        push_constant_ranges.push_back(range);

        hash_combine(hash, push_constant.hash());
    }

    const PipelineLayoutHandle handle = PipelineLayoutHandle{hash};

    if (contains(handle))
    {
        MIZU_LOG_WARNING("Pipeline layout with handle '{}' has already been created", hash);
        return handle;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
    pipeline_layout_create_info.pSetLayouts = set_layouts.data();
    pipeline_layout_create_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
    pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges.data();

    VkPipelineLayout layout;
    vkCreatePipelineLayout(VulkanContext.device->handle(), &pipeline_layout_create_info, nullptr, &layout);

    m_cache.insert({handle, layout});

    if (desc.push_constant.has_value())
    {
        m_push_constant_item_cache.insert({handle, *desc.push_constant});
    }

    return handle;
}

VkPipelineLayout VulkanPipelineLayoutCache::get(PipelineLayoutHandle handle) const
{
    MIZU_ASSERT(contains(handle), "VulkanPipelineLayoutCache does not contain layout with handle {}", handle);
    return m_cache.find(handle)->second;
}

bool VulkanPipelineLayoutCache::contains(PipelineLayoutHandle handle) const
{
    return m_cache.find(handle) != m_cache.end();
}

std::optional<PushConstantItem> VulkanPipelineLayoutCache::get_push_constant_item(PipelineLayoutHandle handle) const
{
    const auto it = m_push_constant_item_cache.find(handle);
    if (it != m_push_constant_item_cache.end())
    {
        return it->second;
    }

    return {};
}

} // namespace Mizu::Vulkan