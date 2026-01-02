#include "vulkan_resource_group.h"

#include "base/debug/logging.h"

#include "vulkan_acceleration_structure.h"
#include "vulkan_buffer_resource.h"
#include "vulkan_context.h"
#include "vulkan_descriptors.h"
#include "vulkan_resource_view.h"
#include "vulkan_sampler_state.h"
#include "vulkan_shader.h"

namespace Mizu::Vulkan
{

#define BUILDER_ITEM_TYPE_CASE(_type, _vector) \
    else if (item.is_type<_type>())            \
    {                                          \
        _vector.push_back(item);               \
    }

VulkanResourceGroup::VulkanResourceGroup(ResourceGroupBuilder builder) : m_builder(std::move(builder))
{
    std::vector<ResourceGroupItem> texture_srvs;
    std::vector<ResourceGroupItem> texture_uavs;

    std::vector<ResourceGroupItem> constant_buffers;

    std::vector<ResourceGroupItem> buffer_srvs;
    std::vector<ResourceGroupItem> buffer_uavs;

    std::vector<ResourceGroupItem> samplers;

    std::vector<ResourceGroupItem> acceleration_structures;

    for (const ResourceGroupItem& item : m_builder.get_resources())
    {
        if (false)
        {
        }
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::TextureSrvT, texture_srvs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::TextureUavT, texture_uavs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::ConstantBufferT, constant_buffers)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::BufferSrvT, buffer_srvs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::BufferUavT, buffer_uavs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::SamplerT, samplers)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::RtxAccelerationStructureT, acceleration_structures)
        else
        {
            MIZU_UNREACHABLE("ResourceGroupItemT not implemented");
        }
    }

    auto vk_builder =
        VulkanDescriptorBuilder::begin(VulkanContext.layout_cache.get(), VulkanContext.descriptor_pool.get());

    std::vector<VkDescriptorImageInfo> image_infos;
    image_infos.reserve(texture_srvs.size() + texture_uavs.size());

    std::vector<VkDescriptorBufferInfo> buffer_infos;
    buffer_infos.reserve(constant_buffers.size() + buffer_srvs.size() + buffer_uavs.size());

    for (const ResourceGroupItem& info : texture_srvs)
    {
        const ResourceView& srv = info.as_type<ResourceGroupItem::TextureSrvT>().value;
        MIZU_ASSERT(srv.view_type == ResourceViewType::ShaderResourceView, "Invalid resource view type for TextureSrv");

        const VulkanImageResourceView* internal_view = get_internal_image_resource_view(srv);

        VkDescriptorImageInfo image_info{};
        image_info.imageView = internal_view->handle;
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        image_infos.push_back(image_info);

        const uint32_t binding = info.binding;
        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_image(binding, &image_infos[image_infos.size() - 1], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stage);
    }

    for (const ResourceGroupItem& info : buffer_srvs)
    {
        const ResourceView& srv = info.as_type<ResourceGroupItem::BufferSrvT>().value;
        MIZU_ASSERT(srv.view_type == ResourceViewType::ShaderResourceView, "Invalid resource view type for BufferSrv");

        const VulkanBufferResourceView* internal_view = get_internal_buffer_resource_view(srv);

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = internal_view->handle;
        buffer_info.offset = internal_view->offset;
        buffer_info.range = internal_view->size;

        buffer_infos.push_back(buffer_info);

        const uint32_t binding = info.binding;
        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_buffer(
            binding, &buffer_infos[buffer_infos.size() - 1], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);
    }

    for (const ResourceGroupItem& info : texture_uavs)
    {
        const ResourceView& uav = info.as_type<ResourceGroupItem::TextureUavT>().value;
        MIZU_ASSERT(
            uav.view_type == ResourceViewType::UnorderedAccessView, "Invalid resource view type for TextureUav");

        const VulkanImageResourceView* internal_view = get_internal_image_resource_view(uav);

        VkDescriptorImageInfo image_info{};
        image_info.imageView = internal_view->handle;
        image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        image_infos.push_back(image_info);

        const uint32_t binding = info.binding;
        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_image(binding, &image_infos[image_infos.size() - 1], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stage);
    }

    for (const ResourceGroupItem& info : buffer_uavs)
    {
        const ResourceView& uav = info.as_type<ResourceGroupItem::BufferUavT>().value;
        MIZU_ASSERT(uav.view_type == ResourceViewType::UnorderedAccessView, "Invalid resource view type for BufferUav");

        const VulkanBufferResourceView* internal_view = get_internal_buffer_resource_view(uav);

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = internal_view->handle;
        buffer_info.offset = internal_view->offset;
        buffer_info.range = internal_view->size;

        buffer_infos.push_back(buffer_info);

        const uint32_t binding = info.binding;
        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_buffer(
            binding, &buffer_infos[buffer_infos.size() - 1], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);
    }

    for (const ResourceGroupItem& info : constant_buffers)
    {
        const ResourceView& cbv = info.as_type<ResourceGroupItem::ConstantBufferT>().value;
        MIZU_ASSERT(
            cbv.view_type == ResourceViewType::ConstantBufferView, "Invalid resource view type for ConstantBuffer");

        const VulkanBufferResourceView* internal_view = get_internal_buffer_resource_view(cbv);

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = internal_view->handle;
        buffer_info.offset = internal_view->offset;
        buffer_info.range = internal_view->size;

        buffer_infos.push_back(buffer_info);

        const uint32_t binding = info.binding;
        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_buffer(
            binding, &buffer_infos[buffer_infos.size() - 1], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage);
    }

    // Build samplers
    std::vector<VkDescriptorImageInfo> sampler_infos;
    sampler_infos.reserve(samplers.size());

    for (const ResourceGroupItem& info : samplers)
    {
        const auto& vk_sampler =
            std::dynamic_pointer_cast<VulkanSamplerState>(info.as_type<ResourceGroupItem::SamplerT>().value);

        VkDescriptorImageInfo image_info{};
        image_info.sampler = vk_sampler->handle();

        sampler_infos.push_back(image_info);

        const uint32_t binding = info.binding;
        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_sampler(binding, &sampler_infos[sampler_infos.size() - 1], VK_DESCRIPTOR_TYPE_SAMPLER, stage);
    }

    // Build acceleration structures
    std::vector<VkWriteDescriptorSetAccelerationStructureKHR> acceleration_structure_infos;
    acceleration_structure_infos.reserve(acceleration_structures.size());

    for (const ResourceGroupItem& info : acceleration_structures)
    {
        const auto& vk_as = std::dynamic_pointer_cast<VulkanAccelerationStructure>(
            info.as_type<ResourceGroupItem::RtxAccelerationStructureT>().value);

        const VkAccelerationStructureKHR& handle = vk_as->handle();

        VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_info{};
        acceleration_structure_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        acceleration_structure_info.accelerationStructureCount = 1;
        acceleration_structure_info.pAccelerationStructures = &handle;

        acceleration_structure_infos.push_back(acceleration_structure_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_acceleration_structure(
            info.binding,
            &acceleration_structure_infos[acceleration_structure_infos.size() - 1],
            VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            stage);
    }

    MIZU_VERIFY(vk_builder.build(m_descriptor_set, m_descriptor_set_layout), "Failed to build descriptor set");
}

VulkanResourceGroup::~VulkanResourceGroup()
{
    VulkanContext.descriptor_pool->free(m_descriptor_set);
}

size_t VulkanResourceGroup::get_hash() const
{
    return m_builder.get_hash();
}

} // namespace Mizu::Vulkan
