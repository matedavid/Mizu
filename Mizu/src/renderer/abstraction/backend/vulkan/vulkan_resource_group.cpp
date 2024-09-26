#include "vulkan_resource_group.h"

#include "renderer/abstraction/backend/vulkan/vulkan_buffers.h"
#include "renderer/abstraction/backend/vulkan/vulkan_descriptors.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image.h"
#include "renderer/abstraction/backend/vulkan/vulkan_shader.h"
#include "renderer/abstraction/backend/vulkan/vulkan_texture.h"

#include "utility/assert.h"

namespace Mizu::Vulkan {

void VulkanResourceGroup::add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) {
    const auto native_texture = std::dynamic_pointer_cast<VulkanTexture2D>(texture);

    m_image_info.insert({std::string{name}, native_texture});
}

void VulkanResourceGroup::add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) {
    const auto native_ubo = std::dynamic_pointer_cast<VulkanUniformBuffer>(ubo);

    m_buffer_info.insert({std::string{name}, native_ubo});
}

bool VulkanResourceGroup::bake(const std::shared_ptr<IShader>& shader, uint32_t set) {
    const auto native_shader = std::dynamic_pointer_cast<VulkanShaderBase>(shader);

    const auto descriptors_in_set = native_shader->get_properties_in_set(set);

    uint32_t num_sampled_images = 0;
    uint32_t num_storage_images = 0;
    uint32_t num_buffers = 0;

    std::unordered_map<std::string, bool> used_descriptors;
    for (const auto& info : descriptors_in_set) {
        if (std::holds_alternative<ShaderTextureProperty>(info.value)) {
            const auto& value = std::get<ShaderTextureProperty>(info.value);

            switch (value.type) {
            case ShaderTextureProperty::Type::Sampled:
                num_sampled_images += 1;
                break;
            case ShaderTextureProperty::Type::Separate:
                // TODO:
                break;
            case ShaderTextureProperty::Type::Storage:
                num_storage_images += 1;
                break;
            }

        } else if (std::holds_alternative<ShaderBufferProperty>(info.value)) {
            // const auto value = std::get<ShaderBufferProperty>(info.value);
            num_buffers += 1;
        }

        used_descriptors.insert({info.name, false});
    }

    VulkanDescriptorPool::PoolSize pool_size;
    if (num_sampled_images != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, num_sampled_images);
    if (num_storage_images != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, num_storage_images);
    if (num_buffers != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, num_buffers);

    m_descriptor_pool = std::make_shared<VulkanDescriptorPool>(pool_size, 1);

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

        const VkShaderStageFlagBits stage = *native_shader->get_property_stage(name);
        const VkDescriptorType vulkan_type = VulkanShaderBase::get_vulkan_descriptor_type(info->value);

        builder = builder.bind_image(info->binding_info.binding, &image_info, vulkan_type, stage);
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

        const VkShaderStageFlagBits stage = *native_shader->get_property_stage(name);
        builder =
            builder.bind_buffer(info->binding_info.binding, &buffer_info, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage);
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

std::optional<ShaderProperty> VulkanResourceGroup::get_descriptor_info(
    const std::string& name,
    uint32_t set,
    VkDescriptorType type,
    const std::shared_ptr<VulkanShaderBase>& shader) {
    auto info = shader->get_property(name);
    if (!info.has_value()) {
        MIZU_LOG_WARNING("Descriptor with name {} does not exist ", name);
        return std::nullopt;
    }

    const VkDescriptorType vulkan_type = VulkanShaderBase::get_vulkan_descriptor_type(info->value);

    /*
    const VkDescriptorType vulkan_type = [&]() -> VkDescriptorType {
        if (std::holds_alternative<ShaderTextureProperty>(info->value)) {
            const auto texture_prop = std::get<ShaderTextureProperty>(info->value);
            switch (texture_prop.type) {
            case ShaderTextureProperty::Type::Sampled:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case ShaderTextureProperty::Type::Separate:
                // TODO: Really, no idea
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case ShaderTextureProperty::Type::Storage:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            }
        } else if (std::holds_alternative<ShaderBufferProperty>(info->value)) {
            const auto buffer_prop = std::get<ShaderBufferProperty>(info->value);
            switch (buffer_prop.type) {
            case ShaderBufferProperty::Type::Uniform:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case ShaderBufferProperty::Type::Storage:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            }
        }

        MIZU_UNREACHABLE("");
    }();
     */

    // if (vulkan_type != type) {
    //     MIZU_LOG_WARNING("Descriptor with name {} is not of type {}", name, static_cast<uint32_t>(type));
    //     return std::nullopt;
    //

    if (info->binding_info.set != set) {
        MIZU_LOG_WARNING(
            "Descriptor with name {} is not in correct descriptor set ({} != {})", name, info->binding_info.set, set);
        return std::nullopt;
    }

    return info;
}

} // namespace Mizu::Vulkan
