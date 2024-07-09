#include "vulkan_resource_group.h"

#include "renderer/abstraction/backend/vulkan/vulkan_buffers.h"
#include "renderer/abstraction/backend/vulkan/vulkan_descriptors.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image.h"
#include "renderer/abstraction/backend/vulkan/vulkan_shader.h"
#include "renderer/abstraction/backend/vulkan/vulkan_texture.h"

namespace Mizu::Vulkan {

void VulkanResourceGroup::add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) {
    const auto native_texture = std::dynamic_pointer_cast<VulkanTexture2D>(texture);

    m_image_info.insert({std::string{name}, native_texture});
}

void VulkanResourceGroup::add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) {
    const auto native_ubo = std::dynamic_pointer_cast<VulkanUniformBuffer>(ubo);

    m_buffer_info.insert({std::string{name}, native_ubo});
}

bool VulkanResourceGroup::bake(const std::shared_ptr<GraphicsShader>& shader, uint32_t set) {
    const auto native_shader = std::dynamic_pointer_cast<VulkanGraphicsShader>(shader);

    VulkanDescriptorPool::PoolSize pool_size;
    if (!m_image_info.empty())
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_image_info.size());
    if (!m_buffer_info.empty())
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_buffer_info.size());

    m_descriptor_pool = std::make_shared<VulkanDescriptorPool>(pool_size, 1);

    const auto descriptors_in_set = native_shader->get_descriptors_in_set(set);

    std::unordered_map<std::string, bool> used_descriptors;
    for (const auto& info : descriptors_in_set) {
        used_descriptors.insert({info.name, false});
    }

    auto builder = VulkanDescriptorBuilder::begin(VulkanContext.layout_cache.get(), m_descriptor_pool.get());

    // Build images
    for (const auto& [name, texture] : m_image_info) {
        const auto info = get_descriptor_info(name, set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, native_shader);

        if (!info.has_value())
            return false;

        used_descriptors[name] = true;

        VkDescriptorImageInfo image_info{};
        image_info.sampler = texture->get_sampler();
        image_info.imageView = texture->get_image()->get_image_view();
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        builder = builder.bind_image(info->binding, &image_info, info->type, info->stage);
    }

    // Build uniform buffers
    for (const auto& [name, ubo] : m_buffer_info) {
        const auto info = get_descriptor_info(name, set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, native_shader);

        if (!info.has_value())
            return false;

        used_descriptors[name] = true;

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = ubo->handle();
        buffer_info.offset = 0;
        buffer_info.range = ubo->size();

        builder = builder.bind_buffer(info->binding, &buffer_info, info->type, info->stage);
    }

    bool all_descriptors_bound = true;
    for (const auto& [name, used] : used_descriptors) {
        if (!used) {
            MIZU_LOG_ERROR("Descriptor with name {} has not been bound", name);
            all_descriptors_bound = false;
        }
    }

    if (!all_descriptors_bound)
        return false;

    m_currently_baked_set_num = set;

    return builder.build(m_set, m_layout);
}

std::optional<VulkanDescriptorInfo> VulkanResourceGroup::get_descriptor_info(
    std::string name,
    uint32_t set,
    VkDescriptorType type,
    const std::shared_ptr<VulkanGraphicsShader>& shader) {
    auto info = shader->get_descriptor_info(name);
    if (!info.has_value()) {
        MIZU_LOG_WARNING("Descriptor with name {} does not exist ", name);
        return std::nullopt;
    }

    if (info->type != type) {
        MIZU_LOG_WARNING("Descriptor with name {} is not of type {}", name, static_cast<uint32_t>(type));
        return std::nullopt;
    }

    if (info->set != set) {
        MIZU_LOG_WARNING("Descriptor with name {} is not in correct descriptor set ({} != {})", name, info->set, set);
        return std::nullopt;
    }

    return info;
}

} // namespace Mizu::Vulkan