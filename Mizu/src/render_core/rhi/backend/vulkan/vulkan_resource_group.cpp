#include "vulkan_resource_group.h"

#include "render_core/resources/buffers.h"

#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_descriptors.h"
#include "render_core/rhi/backend/vulkan/vulkan_resource_view.h"
#include "render_core/rhi/backend/vulkan/vulkan_sampler_state.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

#include "utility/logging.h"

namespace Mizu::Vulkan
{

void VulkanResourceGroup::add_resource(std::string_view name, std::shared_ptr<ImageResourceView> image_resource)
{
    MIZU_ASSERT(!m_image_resource_view_info.contains(std::string(name)),
                "Image resource view with name {} already exists",
                name);

    const auto native_image_resource = std::dynamic_pointer_cast<VulkanImageResourceView>(image_resource);
    m_image_resource_view_info.insert({std::string(name), native_image_resource});
}

void VulkanResourceGroup::add_resource(std::string_view name, std::shared_ptr<BufferResource> buffer_resource)
{
    MIZU_ASSERT(
        !m_buffer_resource_info.contains(std::string(name)), "Buffer resource with name {} already exists", name);

    const auto native_buffer_resource = std::dynamic_pointer_cast<VulkanBufferResource>(buffer_resource);
    m_buffer_resource_info.insert({std::string{name}, native_buffer_resource});
}

void VulkanResourceGroup::add_resource(std::string_view name, std::shared_ptr<SamplerState> sampler_state)
{
    MIZU_ASSERT(!m_sampler_state_info.contains(std::string(name)), "Sampler state with name {} already exists", name);

    const auto native_sampler_state = std::dynamic_pointer_cast<VulkanSamplerState>(sampler_state);
    m_sampler_state_info.insert({std::string(name), native_sampler_state});
}

size_t VulkanResourceGroup::get_hash() const
{
    std::hash<std::string> string_hasher;
    std::hash<uint32_t> uint32_hasher;
    std::hash<uint64_t> uint64_hasher;

    std::hash<ImageResourceView*> image_hasher;
    std::hash<BufferResource*> buffer_hasher;
    std::hash<SamplerState*> sampler_state_hasher;

    size_t hash = 0;
    for (const auto& [name, resource] : m_image_resource_view_info)
    {
        hash ^= string_hasher(name) ^ uint32_hasher(static_cast<uint32_t>(resource->get_format()))
                ^ image_hasher(resource.get());
    }

    for (const auto& [name, resource] : m_buffer_resource_info)
    {
        hash ^= string_hasher(name) ^ uint64_hasher(resource->get_size())
                ^ uint32_hasher(static_cast<uint32_t>(resource->get_type())) ^ buffer_hasher(resource.get());
    }

    for (const auto& [name, resource] : m_sampler_state_info)
    {
        hash ^= string_hasher(name) ^ sampler_state_hasher(resource.get());
    }

    return hash;
}

bool VulkanResourceGroup::bake(const IShader& shader, uint32_t set)
{
    const VulkanShaderBase& native_shader = dynamic_cast<const VulkanShaderBase&>(shader);

    const auto descriptors_in_set = native_shader.get_properties_in_set(set);

    uint32_t num_sampled_images = 0;
    uint32_t num_separate_images = 0;
    uint32_t num_storage_images = 0;
    uint32_t num_buffers = 0;
    uint32_t num_samplers = 0;

    std::unordered_map<std::string, bool> used_descriptors;
    for (const auto& info : descriptors_in_set)
    {
        if (std::holds_alternative<ShaderImageProperty>(info.value))
        {
            const auto& value = std::get<ShaderImageProperty>(info.value);

            switch (value.type)
            {
            case ShaderImageProperty::Type::Sampled:
                num_sampled_images += 1;
                break;
            case ShaderImageProperty::Type::Separate:
                num_separate_images += 1;
                break;
            case ShaderImageProperty::Type::Storage:
                num_storage_images += 1;
                break;
            }
        }
        else if (std::holds_alternative<ShaderBufferProperty>(info.value))
        {
            num_buffers += 1;
        }
        else if (std::holds_alternative<ShaderSamplerProperty>(info.value))
        {
            num_samplers += 1;
        }

        used_descriptors.insert({info.name, false});
    }

    VulkanDescriptorPool::PoolSize pool_size;
    if (num_sampled_images != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, num_sampled_images);
    if (num_separate_images != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, num_separate_images);
    if (num_storage_images != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, num_storage_images);
    if (num_buffers != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, num_buffers);
    if (num_samplers != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_SAMPLER, num_samplers);

    m_descriptor_pool = std::make_shared<VulkanDescriptorPool>(pool_size, 1);

    auto builder = VulkanDescriptorBuilder::begin(VulkanContext.layout_cache.get(), m_descriptor_pool.get());

    // Build images
    std::vector<VkDescriptorImageInfo> image_infos;
    image_infos.reserve(m_image_resource_view_info.size());

    for (const auto& [name, view] : m_image_resource_view_info)
    {
        const auto info = get_descriptor_info(name, set, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, native_shader);

        if (!info.has_value())
        {
            MIZU_LOG_ERROR("Could not find image with name {} in shader", name);
            continue;
        }

        used_descriptors[name] = true;

        VkDescriptorImageInfo image_info{};
        image_info.imageView = view->handle();
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // TODO: Check if this is good idea
        if (view->get_image_usage() & ImageUsageBits::Storage)
        {
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }

        image_infos.push_back(image_info);

        const VkShaderStageFlagBits stage = *native_shader.get_property_stage(name);
        const VkDescriptorType vulkan_type = VulkanShaderBase::get_vulkan_descriptor_type(info->value);

        builder =
            builder.bind_image(info->binding_info.binding, &image_infos[image_infos.size() - 1], vulkan_type, stage);
    }

    // Build buffers
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    buffer_infos.reserve(m_buffer_resource_info.size());

    for (const auto& [name, buffer] : m_buffer_resource_info)
    {
        const auto info = get_descriptor_info(name, set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, native_shader);

        if (!info.has_value())
        {
            MIZU_LOG_ERROR("Could not find buffer with name {} in shader", name);
            continue;
        }

        used_descriptors[name] = true;

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = buffer->handle();
        buffer_info.offset = 0;
        buffer_info.range = buffer->get_size();

        buffer_infos.push_back(buffer_info);

        const VkShaderStageFlagBits stage = *native_shader.get_property_stage(name);
        const VkDescriptorType vulkan_type = VulkanShaderBase::get_vulkan_descriptor_type(info->value);

        builder =
            builder.bind_buffer(info->binding_info.binding, &buffer_infos[buffer_infos.size() - 1], vulkan_type, stage);
    }

    // Build samplers
    std::vector<VkDescriptorImageInfo> sampler_infos;
    sampler_infos.reserve(m_sampler_state_info.size());

    for (const auto& [name, sampler] : m_sampler_state_info)
    {
        const auto info = get_descriptor_info(name, set, VK_DESCRIPTOR_TYPE_SAMPLER, native_shader);

        if (!info.has_value())
        {
            MIZU_LOG_ERROR("Could not find sampler with name {} in shader", name);
            continue;
        }

        used_descriptors[name] = true;

        VkDescriptorImageInfo image_info{};
        image_info.sampler = sampler->handle();

        sampler_infos.push_back(image_info);

        const VkShaderStageFlagBits stage = *native_shader.get_property_stage(name);
        const VkDescriptorType vulkan_type = VulkanShaderBase::get_vulkan_descriptor_type(info->value);

        builder = builder.bind_sampler(
            info->binding_info.binding, &sampler_infos[sampler_infos.size() - 1], vulkan_type, stage);
    }

    bool all_descriptors_bound = true;
    for (const auto& [name, used] : used_descriptors)
    {
        if (!used)
        {
            MIZU_LOG_ERROR("Descriptor with name {} has not been bound", name);
            all_descriptors_bound = false;
        }
    }

    if (!all_descriptors_bound)
        return false;

    m_currently_baked_set_num = set;

    return builder.build(m_set, m_layout);
}

std::optional<ShaderProperty> VulkanResourceGroup::get_descriptor_info(const std::string& name,
                                                                       uint32_t set,
                                                                       [[maybe_unused]] VkDescriptorType type,
                                                                       const VulkanShaderBase& shader)
{
    auto info = shader.get_property(name);
    if (!info.has_value())
    {
        MIZU_LOG_WARNING("Descriptor with name {} does not exist ", name);
        return std::nullopt;
    }

    /*
    const VkDescriptorType vulkan_type = [&]() -> VkDescriptorType {
        if (std::holds_alternative<ShaderImageProperty>(info->value)) {
            const auto texture_prop = std::get<ShaderImageProperty>(info->value);
            switch (texture_prop.type) {
            case ShaderImageProperty::Type::Sampled:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case ShaderImageProperty::Type::Separate:
                // TODO: Really, no idea
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case ShaderImageProperty::Type::Storage:
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

    // TODO: Do something with this
    // const VkDescriptorType vulkan_type = VulkanShaderBase::get_vulkan_descriptor_type(info->value);
    // if (vulkan_type != type) {
    //     MIZU_LOG_WARNING("Descriptor with name {} is not of type {}", name, static_cast<uint32_t>(type));
    //     return std::nullopt;

    if (info->binding_info.set != set)
    {
        MIZU_LOG_WARNING(
            "Descriptor with name {} is not in correct descriptor set ({} != {})", name, info->binding_info.set, set);
        return std::nullopt;
    }

    return info;
}

} // namespace Mizu::Vulkan
