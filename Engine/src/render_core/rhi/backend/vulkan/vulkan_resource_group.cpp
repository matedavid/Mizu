#include "vulkan_resource_group.h"

#include "render_core/resources/buffers.h"

#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_descriptors.h"
#include "render_core/rhi/backend/vulkan/vulkan_resource_view.h"
#include "render_core/rhi/backend/vulkan/vulkan_sampler_state.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_acceleration_structure.h"

#include "utility/logging.h"

namespace Mizu::Vulkan
{

VulkanResourceGroup::VulkanResourceGroup(ResourceGroupLayout layout) : m_layout(std::move(layout))
{
    std::vector<LayoutResource> separate_images;
    std::vector<LayoutResource> storage_images;

    std::vector<LayoutResource> uniform_buffer_resources;
    std::vector<LayoutResource> storage_buffer_resources;

    std::vector<LayoutResource> samplers;
    std::vector<LayoutResource> top_level_acceleration_structures;

    for (const LayoutResource& info : m_layout.get_resources())
    {
        if (info.is_type<ImageResourceView>())
        {
            if (info.stage & ShaderType::Compute)
            {
                storage_images.push_back(info);
            }
            else
            {
                separate_images.push_back(info);
            }
        }
        else if (info.is_type<BufferResource>())
        {
            const auto& vk_buffer = std::dynamic_pointer_cast<VulkanBufferResource>(info.as_type<BufferResource>());

            if (vk_buffer->get_usage() & BufferUsageBits::StorageBuffer)
            {
                storage_buffer_resources.push_back(info);
            }
            else
            {
                uniform_buffer_resources.push_back(info);
            }
        }
        else if (info.is_type<SamplerState>())
        {
            samplers.push_back(info);
        }
        else if (info.is_type<TopLevelAccelerationStructure>())
        {
            top_level_acceleration_structures.push_back(info);
        }
    }

    VulkanDescriptorPool::PoolSize pool_size;

    if (separate_images.size() != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, separate_images.size());
    if (storage_images.size() != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storage_images.size());

    if (uniform_buffer_resources.size() != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniform_buffer_resources.size());
    if (storage_buffer_resources.size() != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, storage_buffer_resources.size());

    if (samplers.size() != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_SAMPLER, samplers.size());

    if (top_level_acceleration_structures.size() != 0)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, top_level_acceleration_structures.size());

    m_descriptor_pool = std::make_shared<VulkanDescriptorPool>(pool_size, 1);

    auto builder = VulkanDescriptorBuilder::begin(VulkanContext.layout_cache.get(), m_descriptor_pool.get());

    // Build images
    std::vector<VkDescriptorImageInfo> image_infos;
    image_infos.reserve(separate_images.size() + storage_images.size());

    for (const LayoutResource& info : separate_images)
    {
        const auto& vk_view = std::dynamic_pointer_cast<VulkanImageResourceView>(info.as_type<ImageResourceView>());

        VkDescriptorImageInfo image_info{};
        image_info.imageView = vk_view->handle();
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        image_infos.push_back(image_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        builder = builder.bind_image(
            info.binding, &image_infos[image_infos.size() - 1], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, stage);
    }

    for (const LayoutResource& info : storage_images)
    {
        const auto& vk_view = std::dynamic_pointer_cast<VulkanImageResourceView>(info.as_type<ImageResourceView>());

        VkDescriptorImageInfo image_info{};
        image_info.imageView = vk_view->handle();
        image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        image_infos.push_back(image_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        builder = builder.bind_image(
            info.binding, &image_infos[image_infos.size() - 1], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stage);
    }

    // Build buffers
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    buffer_infos.reserve(uniform_buffer_resources.size() + storage_buffer_resources.size());

    for (const LayoutResource& info : uniform_buffer_resources)
    {
        const auto& vk_buffer = std::dynamic_pointer_cast<VulkanBufferResource>(info.as_type<BufferResource>());

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = vk_buffer->handle();
        buffer_info.offset = 0;
        buffer_info.range = vk_buffer->get_size();

        buffer_infos.push_back(buffer_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        builder = builder.bind_buffer(
            info.binding, &buffer_infos[buffer_infos.size() - 1], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage);
    }

    for (const LayoutResource& info : storage_buffer_resources)
    {
        const auto& vk_buffer = std::dynamic_pointer_cast<VulkanBufferResource>(info.as_type<BufferResource>());

        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = vk_buffer->handle();
        buffer_info.offset = 0;
        buffer_info.range = vk_buffer->get_size();

        buffer_infos.push_back(buffer_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        builder = builder.bind_buffer(
            info.binding, &buffer_infos[buffer_infos.size() - 1], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);
    }

    // Build samplers
    std::vector<VkDescriptorImageInfo> sampler_infos;
    sampler_infos.reserve(samplers.size());

    for (const LayoutResource& info : samplers)
    {
        const auto& sampler = info.as_type<SamplerState>();
        const auto& vk_sampler = std::dynamic_pointer_cast<VulkanSamplerState>(sampler);

        VkDescriptorImageInfo image_info{};
        image_info.sampler = vk_sampler->handle();

        sampler_infos.push_back(image_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        builder = builder.bind_sampler(
            info.binding, &sampler_infos[sampler_infos.size() - 1], VK_DESCRIPTOR_TYPE_SAMPLER, stage);
    }

    // Build acceleration structures
    std::vector<VkWriteDescriptorSetAccelerationStructureKHR> acceleration_structure_infos;
    acceleration_structure_infos.reserve(top_level_acceleration_structures.size());

    for (const LayoutResource& info : top_level_acceleration_structures)
    {
        const auto& vk_tlas = std::dynamic_pointer_cast<VulkanTopLevelAccelerationStructure>(
            info.as_type<TopLevelAccelerationStructure>());

        const VkAccelerationStructureKHR& handle = vk_tlas->handle();

        VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_info{};
        acceleration_structure_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        acceleration_structure_info.accelerationStructureCount = 1;
        acceleration_structure_info.pAccelerationStructures = &handle;

        acceleration_structure_infos.push_back(acceleration_structure_info);

        const VkShaderStageFlags stage = VulkanShader::get_vulkan_shader_stage_bits(info.stage);

        builder =
            builder.bind_acceleration_structure(info.binding,
                                                &acceleration_structure_infos[acceleration_structure_infos.size() - 1],
                                                VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                                                stage);
    }

    MIZU_VERIFY(builder.build(m_descriptor_set, m_descriptor_set_layout), "Failed to build descriptor set");
}

} // namespace Mizu::Vulkan
