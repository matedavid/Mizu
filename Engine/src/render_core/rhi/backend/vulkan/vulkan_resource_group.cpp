#include "vulkan_resource_group.h"

#include "base/debug/logging.h"

#include "render_core/resources/buffers.h"

#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_descriptors.h"
#include "render_core/rhi/backend/vulkan/vulkan_resource_view.h"
#include "render_core/rhi/backend/vulkan/vulkan_sampler_state.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_acceleration_structure.h"

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
    std::vector<ResourceGroupItem> structured_buffer_srvs;
    std::vector<ResourceGroupItem> structured_buffer_uavs;

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
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::StructuredBufferSrvT, structured_buffer_srvs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::StructuredBufferUavT, structured_buffer_uavs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::SamplerT, samplers)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::RtxAccelerationStructureT, acceleration_structures)
        else
        {
            MIZU_UNREACHABLE("ResourceGroupItemT not implemented");
        }
    }

    auto vk_builder =
        VulkanDescriptorBuilder::begin(VulkanContext.layout_cache.get(), VulkanContext.descriptor_pool.get());

    // Build images
    std::vector<VkDescriptorImageInfo> image_infos;
    image_infos.reserve(texture_srvs.size() + texture_uavs.size());

    for (const ResourceGroupItem& info : texture_srvs)
    {
        const VulkanShaderResourceView& srv =
            static_cast<const VulkanShaderResourceView&>(*info.as_type<ResourceGroupItem::TextureSrvT>().value);

        VkDescriptorImageInfo image_info{};
        image_info.imageView = srv.get_image_view_handle();
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        image_infos.push_back(image_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_image(
            info.binding, &image_infos[image_infos.size() - 1], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stage);
    }

    for (const ResourceGroupItem& info : texture_uavs)
    {
        const VulkanUnorderedAccessView& uav =
            static_cast<const VulkanUnorderedAccessView&>(*info.as_type<ResourceGroupItem::TextureUavT>().value);

        VkDescriptorImageInfo image_info{};
        image_info.imageView = uav.get_image_view_handle();
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        image_infos.push_back(image_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_image(
            info.binding, &image_infos[image_infos.size() - 1], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stage);
    }

    // Build buffers
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    buffer_infos.reserve(constant_buffers.size() + structured_buffer_srvs.size() + structured_buffer_uavs.size());

    for (const ResourceGroupItem& info : constant_buffers)
    {
        const VulkanConstantBufferView& cbv =
            static_cast<const VulkanConstantBufferView&>(*info.as_type<ResourceGroupItem::ConstantBufferT>().value);
        const VulkanBufferResource& native_buffer = cbv.get_buffer();

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = native_buffer.handle();
        buffer_info.offset = 0;
        buffer_info.range = native_buffer.get_size();

        buffer_infos.push_back(buffer_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_buffer(
            info.binding, &buffer_infos[buffer_infos.size() - 1], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage);
    }

    for (const ResourceGroupItem& info : structured_buffer_srvs)
    {
        const VulkanShaderResourceView& srv = static_cast<const VulkanShaderResourceView&>(
            *info.as_type<ResourceGroupItem::StructuredBufferSrvT>().value);
        const VulkanBufferResource& native_buffer = srv.get_buffer();

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = native_buffer.handle();
        buffer_info.offset = 0;
        buffer_info.range = native_buffer.get_size();

        buffer_infos.push_back(buffer_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_buffer(
            info.binding, &buffer_infos[buffer_infos.size() - 1], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);
    }

    for (const ResourceGroupItem& info : structured_buffer_uavs)
    {
        const VulkanUnorderedAccessView& uav = static_cast<const VulkanUnorderedAccessView&>(
            *info.as_type<ResourceGroupItem::StructuredBufferUavT>().value);
        const VulkanBufferResource& native_buffer = uav.get_buffer();

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = native_buffer.handle();
        buffer_info.offset = 0;
        buffer_info.range = native_buffer.get_size();

        buffer_infos.push_back(buffer_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_buffer(
            info.binding, &buffer_infos[buffer_infos.size() - 1], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);
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

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        vk_builder.bind_sampler(
            info.binding, &sampler_infos[sampler_infos.size() - 1], VK_DESCRIPTOR_TYPE_SAMPLER, stage);
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
