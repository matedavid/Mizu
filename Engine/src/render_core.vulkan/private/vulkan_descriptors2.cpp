#include "vulkan_descriptors2.h"

#include "vulkan_acceleration_structure.h"
#include "vulkan_context.h"
#include "vulkan_resource_view.h"
#include "vulkan_sampler_state.h"
#include "vulkan_shader.h"

namespace Mizu::Vulkan
{

//
// VulkanDescriptorSet
//

VulkanDescriptorSet::VulkanDescriptorSet(
    VkDescriptorSet descriptor_set,
    VulkanDescriptorManager& manager,
    DescriptorSetAllocationType type)
    : m_descriptor_set(descriptor_set)
    , m_manager(manager)
    , m_type(type)
{
}

VulkanDescriptorSet::~VulkanDescriptorSet()
{
    switch (m_type)
    {
    case DescriptorSetAllocationType::Transient:
#if MIZU_VULKAN_VALIDATIONS_ENABLED
        m_manager.transient_descriptor_set_freed(this);
#endif
        break;
    case DescriptorSetAllocationType::Persistent:
        m_manager.free_persistent(m_descriptor_set);
        break;
    case DescriptorSetAllocationType::Bindless:
        break;
    }
}

void VulkanDescriptorSet::update(std::span<const WriteDescriptor> writes, uint32_t array_offset)
{
    std::vector<VkDescriptorBufferInfo> buffer_infos;
    buffer_infos.reserve(writes.size());
    std::vector<VkDescriptorImageInfo> image_infos;
    image_infos.reserve(writes.size());
    std::vector<VkWriteDescriptorSetAccelerationStructureKHR> acceleration_structure_infos;
    acceleration_structure_infos.reserve(writes.size());

    // Because VkWriteDescriptorSetAccelerationStructureKHR gets a pointer instead of the handle itself
    std::vector<VkAccelerationStructureKHR> acceleration_structure_handles;
    acceleration_structure_handles.reserve(writes.size());

    std::vector<WriteDescriptor> writes_vec(writes.begin(), writes.end());
    std::sort(writes_vec.begin(), writes_vec.end(), [](const WriteDescriptor& a, const WriteDescriptor& b) {
        return a.binding < b.binding;
    });

    std::vector<VkWriteDescriptorSet> write_descriptor_sets;
    write_descriptor_sets.reserve(writes.size());

    const auto can_merge = [](const VkWriteDescriptorSet& write, uint32_t binding, VkDescriptorType vk_type) -> bool {
        return write.dstBinding == binding && write.descriptorType == vk_type;
    };

    const auto is_image_type = [](VkDescriptorType vk_type) -> bool {
        return vk_type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE || vk_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
               || vk_type == VK_DESCRIPTOR_TYPE_SAMPLER;
    };

    const auto is_buffer_type = [](VkDescriptorType vk_type) -> bool {
        return vk_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || vk_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    };

    uint32_t current_buffer_info_offset = 0;
    uint32_t current_image_info_offset = 0;
    uint32_t current_acceleration_structure_info_offset = 0;

    for (const WriteDescriptor& w : writes_vec)
    {
        switch (w.type)
        {
        case ShaderResourceType::TextureSrv: {
            const ImageResourceView& view = std::get<ImageResourceView>(w.value);

            VulkanImageResource& native_image = static_cast<VulkanImageResource&>(*view.image);
            const VulkanImageResourceView native_view = native_image.as_srv(view.desc);

            VkDescriptorImageInfo image_info{};
            image_info.imageView = native_view.handle;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            image_infos.push_back(image_info);
            break;
        }
        case ShaderResourceType::TextureUav: {
            const ImageResourceView& view = std::get<ImageResourceView>(w.value);

            VulkanImageResource& native_image = static_cast<VulkanImageResource&>(*view.image);
            const VulkanImageResourceView native_view = native_image.as_uav(view.desc);

            VkDescriptorImageInfo image_info{};
            image_info.imageView = native_view.handle;
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            image_infos.push_back(image_info);
            break;
        }
        case ShaderResourceType::StructuredBufferSrv:
        case ShaderResourceType::ByteAddressBufferSrv: {
            const BufferResourceView& view = std::get<BufferResourceView>(w.value);

            VulkanBufferResource& native_buffer = static_cast<VulkanBufferResource&>(*view.buffer);
            const VulkanBufferResourceView native_view = native_buffer.as_srv(view.desc);

            VkDescriptorBufferInfo buffer_info{};
            buffer_info.buffer = native_view.handle;
            buffer_info.offset = native_view.offset;
            buffer_info.range = native_view.size;

            buffer_infos.push_back(buffer_info);
            break;
        }
        case ShaderResourceType::StructuredBufferUav:
        case ShaderResourceType::ByteAddressBufferUav: {
            const BufferResourceView& view = std::get<BufferResourceView>(w.value);

            VulkanBufferResource& native_buffer = static_cast<VulkanBufferResource&>(*view.buffer);
            const VulkanBufferResourceView native_view = native_buffer.as_uav(view.desc);

            VkDescriptorBufferInfo buffer_info{};
            buffer_info.buffer = native_view.handle;
            buffer_info.offset = native_view.offset;
            buffer_info.range = native_view.size;

            buffer_infos.push_back(buffer_info);
            break;
        }
        case ShaderResourceType::ConstantBuffer: {
            const BufferResourceView& view = std::get<BufferResourceView>(w.value);

            VulkanBufferResource& native_buffer = static_cast<VulkanBufferResource&>(*view.buffer);
            const VulkanBufferResourceView native_view = native_buffer.as_cbv(view.desc);

            VkDescriptorBufferInfo buffer_info{};
            buffer_info.buffer = native_view.handle;
            buffer_info.offset = native_view.offset;
            buffer_info.range = native_view.size;

            buffer_infos.push_back(buffer_info);
            break;
        }
        case ShaderResourceType::SamplerState: {
            const std::shared_ptr<SamplerState>& sampler = std::get<std::shared_ptr<SamplerState>>(w.value);
            const VulkanSamplerState& native_sampler = static_cast<const VulkanSamplerState&>(*sampler);

            VkDescriptorImageInfo sampler_info{};
            sampler_info.sampler = native_sampler.handle();

            image_infos.push_back(sampler_info);
            break;
        }
        case ShaderResourceType::AccelerationStructure: {
            const AccelerationStructureView& view = std::get<AccelerationStructureView>(w.value);

            VulkanAccelerationStructure& native_accel_struct =
                static_cast<VulkanAccelerationStructure&>(*view.accel_struct);
            const VulkanAccelerationStructureResourceView native_view = native_accel_struct.as_srv();
            acceleration_structure_handles.push_back(native_view.handle);

            VkWriteDescriptorSetAccelerationStructureKHR acceleration_structure_info{};
            acceleration_structure_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            acceleration_structure_info.accelerationStructureCount = 1;
            acceleration_structure_info.pAccelerationStructures = &acceleration_structure_handles.back();

            acceleration_structure_infos.push_back(acceleration_structure_info);
            break;
        }
        case ShaderResourceType::PushConstant:
            MIZU_UNREACHABLE("PushConstant is invalid in this context");
            continue;
        }

        const VkDescriptorType vk_type = VulkanShader::get_vulkan_descriptor_type(w.type);

        if (write_descriptor_sets.empty() || !can_merge(write_descriptor_sets.back(), w.binding, vk_type))
        {
            VkWriteDescriptorSet& write_set = write_descriptor_sets.emplace_back();
            write_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_set.dstSet = m_descriptor_set;
            write_set.dstBinding = get_binding_with_offset(w.binding, w.type);
            write_set.dstArrayElement = array_offset;
            write_set.descriptorCount = 1;
            write_set.descriptorType = vk_type;

            if (is_buffer_type(vk_type))
            {
                write_set.pBufferInfo = buffer_infos.data() + current_buffer_info_offset;
            }
            else if (is_image_type(vk_type))
            {
                write_set.pImageInfo = image_infos.data() + current_image_info_offset;
            }
            else if (vk_type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
            {
                write_set.pNext = acceleration_structure_infos.data() + current_acceleration_structure_info_offset;
            }
            else
            {
                MIZU_UNREACHABLE("Invalid or unimplemented resource type");
            }
        }
        else if (can_merge(write_descriptor_sets.back(), w.binding, vk_type))
        {
            VkWriteDescriptorSet& last_write_set = write_descriptor_sets.back();
            last_write_set.descriptorCount += 1;
        }

        if (is_buffer_type(vk_type))
        {
            current_buffer_info_offset += 1;
        }
        else if (is_image_type(vk_type))
        {
            current_image_info_offset += 1;
        }
        else if (vk_type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
            current_acceleration_structure_info_offset += 1;
        }
        else
        {
            MIZU_UNREACHABLE("Invalid or unimplemented resource type");
        }
    }

    vkUpdateDescriptorSets(
        VulkanContext.device->handle(),
        static_cast<uint32_t>(write_descriptor_sets.size()),
        write_descriptor_sets.data(),
        0,
        nullptr);
}

//
// VulkanDescriptorManager
//

VulkanDescriptorManager::VulkanDescriptorManager(const VulkanDescriptorManagerDescription& desc)
{
    MIZU_ASSERT(desc.num_transient_pools >= 1, "Minumum one transient pool is needed");

    static constexpr uint32_t MAX_DESCRIPTOR_SETS = 100;

    for (uint32_t i = 0; i < desc.num_transient_pools; ++i)
    {
        VkDescriptorPoolCreateInfo transient_create_info{};
        transient_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        transient_create_info.pNext = nullptr;
        transient_create_info.flags = 0;
        transient_create_info.maxSets = MAX_DESCRIPTOR_SETS;
        transient_create_info.poolSizeCount = static_cast<uint32_t>(desc.transient_pool_sizes.size());
        transient_create_info.pPoolSizes = desc.transient_pool_sizes.data();

        VkDescriptorPool& descriptor_pool = m_transient_descriptor_pools.emplace_back();
        VK_CHECK(
            vkCreateDescriptorPool(VulkanContext.device->handle(), &transient_create_info, nullptr, &descriptor_pool));
    }

    {
        VkDescriptorPoolCreateInfo persistent_create_info{};
        persistent_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        persistent_create_info.pNext = nullptr;
        persistent_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        persistent_create_info.maxSets = MAX_DESCRIPTOR_SETS;
        persistent_create_info.poolSizeCount = static_cast<uint32_t>(desc.persistent_pool_sizes.size());
        persistent_create_info.pPoolSizes = desc.persistent_pool_sizes.data();

        VK_CHECK(vkCreateDescriptorPool(
            VulkanContext.device->handle(), &persistent_create_info, nullptr, &m_persistent_descriptor_pool));
    }

    {
        VkDescriptorPoolCreateInfo bindless_create_info{};
        bindless_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        bindless_create_info.pNext = nullptr;
        bindless_create_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
        bindless_create_info.maxSets = MAX_DESCRIPTOR_SETS;
        bindless_create_info.poolSizeCount = static_cast<uint32_t>(desc.bindless_pool_sizes.size());
        bindless_create_info.pPoolSizes = desc.bindless_pool_sizes.data();

        VK_CHECK(vkCreateDescriptorPool(
            VulkanContext.device->handle(), &bindless_create_info, nullptr, &m_bindless_descriptor_pool));
    }

#if MIZU_DEBUG
    for (uint32_t i = 0; i < m_transient_descriptor_pools.size(); ++i)
    {
        m_tracked_transient_resources.emplace_back();
    }
#endif
}

VulkanDescriptorManager::~VulkanDescriptorManager()
{
    for (uint32_t i = 0; i < m_transient_descriptor_pools.size(); ++i)
        vkDestroyDescriptorPool(VulkanContext.device->handle(), m_transient_descriptor_pools[i], nullptr);
    vkDestroyDescriptorPool(VulkanContext.device->handle(), m_persistent_descriptor_pool, nullptr);
    vkDestroyDescriptorPool(VulkanContext.device->handle(), m_bindless_descriptor_pool, nullptr);
}

std::shared_ptr<DescriptorSet> VulkanDescriptorManager::allocate_transient(DescriptorSetLayoutHandle layout)
{
    const VkDescriptorSetLayout descriptor_set_layout = VulkanContext.descriptor_set_layout_cache->get(layout);

    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.pNext = nullptr;
    allocate_info.descriptorPool = m_transient_descriptor_pools[m_current_transient_pool_idx];
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_set_layout;

    VkDescriptorSet descriptor_set;
    VK_CHECK(vkAllocateDescriptorSets(VulkanContext.device->handle(), &allocate_info, &descriptor_set));

    const auto vulkan_descriptor_set =
        std::make_shared<VulkanDescriptorSet>(descriptor_set, *this, DescriptorSetAllocationType::Transient);

#if MIZU_VULKAN_VALIDATIONS_ENABLED
    transient_descriptor_set_created(vulkan_descriptor_set.get());
#endif

    return vulkan_descriptor_set;
}

void VulkanDescriptorManager::reset_transient(uint32_t pool_idx)
{
#if MIZU_VULKAN_VALIDATIONS_ENABLED
    if (!m_tracked_transient_resources.empty())
    {
        for (const VulkanDescriptorSet* ds : m_tracked_transient_resources[pool_idx])
        {
            MIZU_LOG_WARNING(
                "Still living DescriptorSet reference with address '{}' when trying to reset the transient "
                "descriptors, this could cause problems if the DescriptorSet is bound after this call.",
                reinterpret_cast<uintptr_t>(ds));
        }

        m_tracked_transient_resources[pool_idx].clear();
    }
#endif

    MIZU_ASSERT(
        pool_idx < m_transient_descriptor_pools.size(),
        "Invalid pool idx {} when number of requested transient pools is {}",
        pool_idx,
        m_transient_descriptor_pools.size());

    m_current_transient_pool_idx = pool_idx;
    VK_CHECK(vkResetDescriptorPool(
        VulkanContext.device->handle(), m_transient_descriptor_pools[m_current_transient_pool_idx], 0));
}

std::shared_ptr<DescriptorSet> VulkanDescriptorManager::allocate_persistent(DescriptorSetLayoutHandle layout)
{
    const VkDescriptorSetLayout descriptor_set_layout = VulkanContext.descriptor_set_layout_cache->get(layout);

    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.pNext = nullptr;
    allocate_info.descriptorPool = m_persistent_descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_set_layout;

    VkDescriptorSet descriptor_set;
    VK_CHECK(vkAllocateDescriptorSets(VulkanContext.device->handle(), &allocate_info, &descriptor_set));

    return std::make_shared<VulkanDescriptorSet>(descriptor_set, *this, DescriptorSetAllocationType::Persistent);
}

void VulkanDescriptorManager::free_persistent(VkDescriptorSet set) const
{
    VK_CHECK(vkFreeDescriptorSets(VulkanContext.device->handle(), m_persistent_descriptor_pool, 1, &set));
}

std::shared_ptr<DescriptorSet> VulkanDescriptorManager::allocate_bindless(
    DescriptorSetLayoutHandle layout,
    uint32_t variable_count)
{
    MIZU_ASSERT(variable_count > 0, "Non-zero variable_count is required when allocating bindless");

    const VkDescriptorSetLayout descriptor_set_layout = VulkanContext.descriptor_set_layout_cache->get(layout);

    const uint32_t max_binding = variable_count - 1;

    VkDescriptorSetVariableDescriptorCountAllocateInfo variable_descriptor_count_allocate_info{};
    variable_descriptor_count_allocate_info.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
    variable_descriptor_count_allocate_info.descriptorSetCount = 1;
    variable_descriptor_count_allocate_info.pDescriptorCounts = &max_binding;

    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.pNext = &variable_descriptor_count_allocate_info;
    allocate_info.descriptorPool = m_bindless_descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_set_layout;

    VkDescriptorSet descriptor_set;
    VK_CHECK(vkAllocateDescriptorSets(VulkanContext.device->handle(), &allocate_info, &descriptor_set));

    return std::make_shared<VulkanDescriptorSet>(descriptor_set, *this, DescriptorSetAllocationType::Bindless);
}

VkDescriptorSetLayout VulkanDescriptorManager::get_descriptor_set_layout(
    std::span<const DescriptorItem> layout,
    DescriptorSetAllocationType type)
{
    MIZU_ASSERT(!layout.empty(), "Can't create descriptor set layout with empty layout");

    // type == DescriptorSetAllocationType::Bindless -> layout.size() == 1
    MIZU_ASSERT(
        type != DescriptorSetAllocationType::Bindless || layout.size() == 1,
        "Currently only supporting bindless descriptor sets with only one binding");

    std::vector<VkDescriptorSetLayoutBinding> layout_bindings{};
    layout_bindings.reserve(layout.size());

    std::vector<VkDescriptorBindingFlags> descriptor_binding_flags{};

    if (type == DescriptorSetAllocationType::Bindless)
    {
        constexpr VkDescriptorBindingFlags bindless_flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
                                                            | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                                                            | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
        descriptor_binding_flags.push_back(bindless_flags);
    }

    for (const DescriptorItem& item : layout)
    {
        uint32_t descriptor_count = item.count;
        if (type == DescriptorSetAllocationType::Bindless)
        {
            // TODO: Fix this, should not be hardcoded here, also defined in vulkan_pipeline_layout.cpp
            descriptor_count = 1024;
        }

        VkDescriptorSetLayoutBinding binding{};
        binding.binding = get_binding_with_offset(item.binding, item.type);
        binding.descriptorType = VulkanShader::get_vulkan_descriptor_type(item.type);
        binding.descriptorCount = descriptor_count;
        binding.stageFlags = VulkanShader::get_vulkan_shader_stage_bits(item.stage);
        binding.pImmutableSamplers = nullptr;

        layout_bindings.push_back(binding);
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info{};
    binding_flags_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags_create_info.pNext = nullptr;
    binding_flags_create_info.bindingCount = static_cast<uint32_t>(descriptor_binding_flags.size());
    binding_flags_create_info.pBindingFlags = descriptor_binding_flags.data();

    VkDescriptorSetLayoutCreateInfo layout_create_info{};
    layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_create_info.pNext = &binding_flags_create_info;
    layout_create_info.flags =
        type == DescriptorSetAllocationType::Bindless ? VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT : 0;
    layout_create_info.bindingCount = static_cast<uint32_t>(layout_bindings.size());
    layout_create_info.pBindings = layout_bindings.data();

    return VulkanContext.layout_cache->create_descriptor_layout(layout_create_info);
}

#if MIZU_VULKAN_VALIDATIONS_ENABLED

void VulkanDescriptorManager::transient_descriptor_set_created(VulkanDescriptorSet* descriptor_set)
{
    m_tracked_transient_resources[m_current_transient_pool_idx].insert(descriptor_set);
}

void VulkanDescriptorManager::transient_descriptor_set_freed(VulkanDescriptorSet* descriptor_set)
{
    MIZU_ASSERT(
        m_tracked_transient_resources[m_current_transient_pool_idx].contains(descriptor_set),
        "Trying to free descriptor set that is not tracked with address '{}' and current transient pool '{}'",
        reinterpret_cast<uintptr_t>(descriptor_set),
        m_current_transient_pool_idx);

    m_tracked_transient_resources[m_current_transient_pool_idx].erase(descriptor_set);
}

#endif

} // namespace Mizu::Vulkan
