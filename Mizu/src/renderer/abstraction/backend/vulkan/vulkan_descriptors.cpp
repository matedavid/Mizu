#include "vulkan_descriptors.h"

#include <algorithm>
#include <ranges>

#include "utility/logging.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"

namespace Mizu::Vulkan {

//
// VulkanDescriptorPool
//

VulkanDescriptorPool::VulkanDescriptorPool(PoolSize size, uint32_t num_sets)
      : m_pool_size(std::move(size)), m_max_sets(num_sets) {
    std::vector<VkDescriptorPoolSize> sizes;
    for (const auto& s : m_pool_size) {
        VkDescriptorPoolSize vk_size{};
        vk_size.type = s.first;
        vk_size.descriptorCount = s.second * num_sets;

        sizes.push_back(vk_size);
    }

    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.maxSets = num_sets;
    info.poolSizeCount = static_cast<uint32_t>(sizes.size());
    info.pPoolSizes = sizes.data();

    vkCreateDescriptorPool(VulkanContext.device->handle(), &info, nullptr, &m_descriptor_pool);
}

VulkanDescriptorPool::~VulkanDescriptorPool() {
    vkDestroyDescriptorPool(VulkanContext.device->handle(), m_descriptor_pool, nullptr);
}

bool VulkanDescriptorPool::allocate(VkDescriptorSetLayout layout, VkDescriptorSet& set) {
    if (m_allocated_sets >= m_max_sets) {
        MIZU_LOG_WARNING("VulkanDescriptorPool has already allocated the maximum number of sets: {}", m_max_sets);
    }

    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.pSetLayouts = &layout;
    info.descriptorPool = m_descriptor_pool;
    info.descriptorSetCount = 1;

    return vkAllocateDescriptorSets(VulkanContext.device->handle(), &info, &set) == VK_SUCCESS;
}

//
// VulkanDescriptorLayoutCache
//

VulkanDescriptorLayoutCache::~VulkanDescriptorLayoutCache() {
    for (const auto& [_, layout] : m_layout_cache) {
        vkDestroyDescriptorSetLayout(VulkanContext.device->handle(), layout, nullptr);
    }
}

VkDescriptorSetLayout VulkanDescriptorLayoutCache::create_descriptor_layout(
    const VkDescriptorSetLayoutCreateInfo& info) {
    DescriptorLayoutInfo layout_info{};
    layout_info.bindings.reserve(info.bindingCount);

    bool is_sorted = true;
    int32_t last_binding = -1;

    for (uint32_t i = 0; i < info.bindingCount; i++) {
        layout_info.bindings.push_back(info.pBindings[i]);

        // check that the bindings are in strict increasing order
        if ((int32_t)info.pBindings[i].binding > last_binding) {
            last_binding = static_cast<int32_t>(info.pBindings[i].binding);
        } else {
            is_sorted = false;
        }
    }

    if (!is_sorted) {
        std::ranges::sort(layout_info.bindings, [](const auto& a, const auto& b) { return a.binding < b.binding; });
    }

    if (m_layout_cache.contains(layout_info)) {
        return m_layout_cache.find(layout_info)->second;
    }

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(VulkanContext.device->handle(), &info, nullptr, &layout));

    m_layout_cache[layout_info] = layout;
    return layout;
}

bool VulkanDescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const {
    if (other.bindings.size() != bindings.size()) {
        return false;
    }

    // compare each of the bindings is the same. Bindings are sorted so they will match
    for (uint32_t i = 0; i < bindings.size(); i++) {
        if (other.bindings[i].binding != bindings[i].binding)
            return false;

        if (other.bindings[i].descriptorType != bindings[i].descriptorType)
            return false;

        if (other.bindings[i].descriptorCount != bindings[i].descriptorCount)
            return false;

        if (other.bindings[i].stageFlags != bindings[i].stageFlags)
            return false;
    }

    return true;
}

size_t VulkanDescriptorLayoutCache::DescriptorLayoutInfo::hash() const {
    size_t result = std::hash<size_t>()(bindings.size());

    for (const VkDescriptorSetLayoutBinding& b : bindings) {
        // pack the binding data into a single int64. Not fully correct but its ok
        const size_t binding_hash =
            b.binding | static_cast<uint32_t>(b.descriptorType << 8) | b.descriptorCount << 16 | b.stageFlags << 24;

        // shuffle the packed binding data and xor it with the main hash
        result ^= std::hash<size_t>()(binding_hash);
    }

    return result;
}

//
// VulkanDescriptorBuilder
//

VulkanDescriptorBuilder VulkanDescriptorBuilder::begin(VulkanDescriptorLayoutCache* cache, VulkanDescriptorPool* pool) {
    VulkanDescriptorBuilder builder{};

    builder.m_cache = cache;
    builder.m_pool = pool;

    return builder;
}

VulkanDescriptorBuilder& VulkanDescriptorBuilder::bind_buffer(uint32_t binding,
                                                              const VkDescriptorBufferInfo* buffer_info,
                                                              VkDescriptorType type,
                                                              VkShaderStageFlags stage_flags,
                                                              uint32_t descriptor_count) {
    // create the descriptor binding for the layout
    VkDescriptorSetLayoutBinding new_binding{};
    new_binding.descriptorCount = descriptor_count;
    new_binding.descriptorType = type;
    new_binding.pImmutableSamplers = nullptr;
    new_binding.stageFlags = stage_flags;
    new_binding.binding = binding;

    bindings.push_back(new_binding);

    // create the descriptor write
    VkWriteDescriptorSet new_write{};
    new_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    new_write.pNext = nullptr;
    new_write.descriptorCount = descriptor_count;
    new_write.descriptorType = type;
    new_write.pBufferInfo = buffer_info;
    new_write.dstBinding = binding;

    writes.push_back(new_write);

    return *this;
}

VulkanDescriptorBuilder& VulkanDescriptorBuilder::bind_image(uint32_t binding,
                                                             const VkDescriptorImageInfo* image_info,
                                                             VkDescriptorType type,
                                                             VkShaderStageFlags stage_flags,
                                                             uint32_t descriptor_count) {
    VkDescriptorSetLayoutBinding new_binding{};
    new_binding.descriptorCount = descriptor_count;
    new_binding.descriptorType = type;
    new_binding.pImmutableSamplers = nullptr;
    new_binding.stageFlags = stage_flags;
    new_binding.binding = binding;

    bindings.push_back(new_binding);

    VkWriteDescriptorSet new_write{};
    new_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    new_write.pNext = nullptr;
    new_write.descriptorCount = descriptor_count;
    new_write.descriptorType = type;
    new_write.pImageInfo = image_info;
    new_write.dstBinding = binding;

    writes.push_back(new_write);

    return *this;
}

bool VulkanDescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout) {
    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.pNext = nullptr;
    layout_info.pBindings = bindings.data();
    layout_info.bindingCount = static_cast<uint32_t>(bindings.size());

    layout = m_cache->create_descriptor_layout(layout_info);

    if (!m_pool->allocate(layout, set)) {
        return false;
    }

    for (VkWriteDescriptorSet& w : writes) {
        w.dstSet = set;
    }

    vkUpdateDescriptorSets(VulkanContext.device->handle(), (uint32_t)writes.size(), writes.data(), 0, nullptr);

    return true;
}

bool VulkanDescriptorBuilder::build(VkDescriptorSet& set) {
    VkDescriptorSetLayout layout;
    return build(set, layout);
}

} // namespace Mizu::Vulkan