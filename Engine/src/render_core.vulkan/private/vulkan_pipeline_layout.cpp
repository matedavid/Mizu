#include "vulkan_pipeline_layout.h"

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"
#include "base/utils/hash.h"

#include "vulkan_context.h"
#include "vulkan_shader.h"

namespace Mizu::Vulkan
{

VulkanPipelineLayoutCache::~VulkanPipelineLayoutCache()
{
    for (const auto& [id, layout] : m_cache)
    {
        vkDestroyPipelineLayout(VulkanContext.device->handle(), layout, nullptr);
    }
}

VkPipelineLayout VulkanPipelineLayoutCache::create(PipelineLayoutId id, const VkPipelineLayoutCreateInfo& create_info)
{
    MIZU_ASSERT(!contains(id), "Pipeline layout cache already contains entry with id: {}", id);

    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(VulkanContext.device->handle(), &create_info, nullptr, &pipeline_layout));

    m_cache.insert({id, pipeline_layout});
    return pipeline_layout;
}

VkPipelineLayout VulkanPipelineLayoutCache::get(PipelineLayoutId id) const
{
    MIZU_ASSERT(contains(id), "Pipeline layout cache does not contain entry with id: {}", id);
    return m_cache.find(id)->second;
}

bool VulkanPipelineLayoutCache::contains(PipelineLayoutId id) const
{
    return m_cache.find(id) != m_cache.end();
}

VkPipelineLayout create_pipeline_layout(std::span<DescriptorBindingInfo> binding_info)
{
    size_t hash = 0;
    for (const DescriptorBindingInfo& info : binding_info)
    {
        hash_combine(hash, info.hash());
    }

    if (VulkanContext.pipeline_layout_cache->contains(hash))
    {
        return VulkanContext.pipeline_layout_cache->get(hash);
    }

    std::unordered_map<uint32_t, std::vector<DescriptorBindingInfo>> m_set_to_binding_infos;

    constexpr size_t MAX_PUSH_CONSTANT_RANGES = 10;
    inplace_vector<VkPushConstantRange, MAX_PUSH_CONSTANT_RANGES> push_constant_ranges;

    for (const DescriptorBindingInfo& binding : binding_info)
    {
        if (binding.type == ShaderResourceType::PushConstant)
            continue;

        auto it = m_set_to_binding_infos.find(binding.binding_info.set);
        if (it == m_set_to_binding_infos.end())
        {
            it = m_set_to_binding_infos.insert({binding.binding_info.set, std::vector<DescriptorBindingInfo>{}}).first;
        }

        it->second.push_back(binding);
    }

    constexpr uint32_t MAX_DESCRIPTOR_SET_LAYOUTS = 10;
    inplace_vector<VkDescriptorSetLayout, MAX_DESCRIPTOR_SET_LAYOUTS> set_layouts;

    for (uint32_t set = 0; set < m_set_to_binding_infos.size(); ++set)
    {
        const std::vector<DescriptorBindingInfo>& bindings = m_set_to_binding_infos[set];

        std::vector<VkDescriptorSetLayoutBinding> layout_bindings{};
        layout_bindings.reserve(bindings.size());

        std::vector<VkDescriptorBindingFlags> layout_binding_flags{};
        layout_binding_flags.reserve(bindings.size());

        bool is_bindless = false;
        for (const DescriptorBindingInfo& binding : bindings)
        {
            uint32_t descriptor_count = binding.size;

            const auto is_array_resource_type = [](ShaderResourceType type) -> bool {
                return type == ShaderResourceType::TextureSrv;
            };

            if (binding.size == 0 && is_array_resource_type(binding.type))
            {
                // TODO: Fix this, should not be hardcoded here, also defined in vulkan_descriptors2.cpp
                descriptor_count = 1024;

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
            layout_binding.binding = binding.binding_info.binding;
            layout_binding.descriptorType = VulkanShader::get_vulkan_descriptor_type(binding.type);
            layout_binding.descriptorCount = descriptor_count;
            layout_binding.stageFlags = VulkanShader::get_vulkan_shader_stage_bits(binding.stage);
            layout_binding.pImmutableSamplers = nullptr;

            layout_bindings.push_back(layout_binding);
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

        VkDescriptorSetLayout layout = VulkanContext.layout_cache->create_descriptor_layout(layout_create_info);
        set_layouts.push_back(layout);
    }

    for (const DescriptorBindingInfo& binding : binding_info)
    {
        if (binding.type != ShaderResourceType::PushConstant)
            continue;

        VkPushConstantRange range{};
        range.stageFlags = VulkanShader::get_vulkan_shader_stage_bits(binding.stage);
        range.offset = 0;
        range.size = binding.size;

        push_constant_ranges.push_back(range);
    }

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{};
    pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_info.setLayoutCount = static_cast<uint32_t>(set_layouts.size());
    pipeline_layout_create_info.pSetLayouts = set_layouts.data();
    pipeline_layout_create_info.pushConstantRangeCount = static_cast<uint32_t>(push_constant_ranges.size());
    pipeline_layout_create_info.pPushConstantRanges = push_constant_ranges.data();

    return VulkanContext.pipeline_layout_cache->create(hash, pipeline_layout_create_info);
}

} // namespace Mizu::Vulkan