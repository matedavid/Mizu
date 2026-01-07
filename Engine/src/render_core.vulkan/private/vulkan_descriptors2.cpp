#include "vulkan_descriptors2.h"

#include "vulkan_acceleration_structure.h"
#include "vulkan_context.h"
#include "vulkan_resource_view.h"
#include "vulkan_sampler_state.h"

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

void VulkanDescriptorSet::update(std::span<WriteDescriptor> writes, uint32_t array_offset)
{
    std::vector<VkDescriptorImageInfo> sampled_image_infos;
    std::vector<VkDescriptorImageInfo> storage_image_infos;

    std::vector<VkDescriptorBufferInfo> uniform_buffer_infos;
    std::vector<VkDescriptorBufferInfo> storage_buffer_infos;

    std::vector<VkDescriptorImageInfo> sampler_state_infos;

    for (const WriteDescriptor& w : writes)
    {
        switch (w.type)
        {
        case ShaderResourceType::TextureSrv: {
            const ResourceView& view = std::get<ResourceView>(w.value);
            const VulkanImageResourceView* internal_view = get_internal_image_resource_view(view);

            VkDescriptorImageInfo image_info{};
            image_info.imageView = internal_view->handle;
            image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            sampled_image_infos.push_back(image_info);
            break;
        }
        case ShaderResourceType::TextureUav: {
            const ResourceView& view = std::get<ResourceView>(w.value);
            const VulkanImageResourceView* internal_view = get_internal_image_resource_view(view);

            VkDescriptorImageInfo image_info{};
            image_info.imageView = internal_view->handle;
            image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            storage_image_infos.push_back(image_info);
            break;
        }
        case ShaderResourceType::StructuredBufferSrv:
        case ShaderResourceType::StructuredBufferUav:
        case ShaderResourceType::ByteAddressBufferSrv:
        case ShaderResourceType::ByteAddressBufferUav: {
            const ResourceView& view = std::get<ResourceView>(w.value);
            const VulkanBufferResourceView* internal_view = get_internal_buffer_resource_view(view);

            VkDescriptorBufferInfo buffer_info{};
            buffer_info.buffer = internal_view->handle;
            buffer_info.offset = internal_view->offset;
            buffer_info.range = internal_view->size;

            storage_buffer_infos.push_back(buffer_info);
            break;
        }
        case ShaderResourceType::ConstantBuffer: {
            const ResourceView& view = std::get<ResourceView>(w.value);
            const VulkanBufferResourceView* internal_view = get_internal_buffer_resource_view(view);

            VkDescriptorBufferInfo buffer_info{};
            buffer_info.buffer = internal_view->handle;
            buffer_info.offset = internal_view->offset;
            buffer_info.range = internal_view->size;

            uniform_buffer_infos.push_back(buffer_info);
            break;
        }
        case ShaderResourceType::SamplerState: {
            const std::shared_ptr<SamplerState>& sampler = std::get<std::shared_ptr<SamplerState>>(w.value);
            const VulkanSamplerState& native_sampler = static_cast<const VulkanSamplerState&>(*sampler);

            VkDescriptorImageInfo sampler_info{};
            sampler_info.sampler = native_sampler.handle();

            sampler_state_infos.push_back(sampler_info);
            break;
        }
        case ShaderResourceType::AccelerationStructure: {
            // TODO:
            break;
        }
        case ShaderResourceType::PushConstant:
            MIZU_UNREACHABLE("Invalid descriptor type");
            break;
        }
    }

    inplace_vector<VkWriteDescriptorSet, 6> write_descriptor_sets;

    VkWriteDescriptorSet template_write{};
    template_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    template_write.dstSet = m_descriptor_set;
    template_write.dstArrayElement = array_offset;

#define ADD_WRITE_DESCRIPTOR(_type, _values, _out)                              \
    if (!_values.empty())                                                       \
    {                                                                           \
        template_write.descriptorType = _type;                                  \
        template_write.descriptorCount = static_cast<uint32_t>(_values.size()); \
        template_write._out = _values.data();                                   \
                                                                                \
        write_descriptor_sets.push_back(template_write);                        \
    }

    ADD_WRITE_DESCRIPTOR(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampled_image_infos, pImageInfo);
    ADD_WRITE_DESCRIPTOR(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storage_image_infos, pImageInfo);
    ADD_WRITE_DESCRIPTOR(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, storage_buffer_infos, pBufferInfo);
    ADD_WRITE_DESCRIPTOR(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniform_buffer_infos, pBufferInfo);
    ADD_WRITE_DESCRIPTOR(VK_DESCRIPTOR_TYPE_SAMPLER, sampler_state_infos, pImageInfo);
    // TODO: ADD_WRITE_DESCRIPTOR for AccelerationStructure

#undef ADD_WRITE_DESCRIPTOR

    /*
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptor_set;
    write.dstBinding = 0;
    write.dstArrayElement = first_slot;
    write.descriptorCount = static_cast<uint32_t>(descriptor_image_infos.size());
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.pImageInfo = descriptor_image_infos.data();
    */

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
    static constexpr uint32_t MAX_DESCRIPTOR_SETS = 100;

    {
        VkDescriptorPoolCreateInfo transient_create_info{};
        transient_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        transient_create_info.pNext = nullptr;
        transient_create_info.flags = 0;
        transient_create_info.maxSets = MAX_DESCRIPTOR_SETS;
        transient_create_info.poolSizeCount = static_cast<uint32_t>(desc.transient_pool_sizes.size());
        transient_create_info.pPoolSizes = desc.transient_pool_sizes.data();

        VK_CHECK(vkCreateDescriptorPool(
            VulkanContext.device->handle(), &transient_create_info, nullptr, &m_transient_descriptor_pool));
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
}

VulkanDescriptorManager::~VulkanDescriptorManager()
{
    vkDestroyDescriptorPool(VulkanContext.device->handle(), m_transient_descriptor_pool, nullptr);
    vkDestroyDescriptorPool(VulkanContext.device->handle(), m_persistent_descriptor_pool, nullptr);
    vkDestroyDescriptorPool(VulkanContext.device->handle(), m_bindless_descriptor_pool, nullptr);
}

std::shared_ptr<DescriptorSet> VulkanDescriptorManager::allocate_transient(std::span<DescriptorItem> layout)
{
    const VkDescriptorSetLayout descriptor_set_layout =
        get_descriptor_set_layout(layout, DescriptorSetAllocationType::Transient);

    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.pNext = nullptr;
    allocate_info.descriptorPool = m_transient_descriptor_pool;
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

void VulkanDescriptorManager::reset_transient()
{
#if MIZU_VULKAN_VALIDATIONS_ENABLED
    if (!m_tracked_transient_resources.empty())
    {
        for (const VulkanDescriptorSet* ds : m_tracked_transient_resources)
        {
            MIZU_LOG_WARNING(
                "Still living DescriptorSet reference with address '{}' when trying to reset the transient "
                "descriptors, this could cause problems if the DescriptorSet is bound after this call.",
                reinterpret_cast<uintptr_t>(ds));
        }

        m_tracked_transient_resources.clear();
    }
#endif

    VK_CHECK(vkResetDescriptorPool(VulkanContext.device->handle(), m_transient_descriptor_pool, 0));
}

std::shared_ptr<DescriptorSet> VulkanDescriptorManager::allocate_persistent(std::span<DescriptorItem> layout)
{
    const VkDescriptorSetLayout descriptor_set_layout =
        get_descriptor_set_layout(layout, DescriptorSetAllocationType::Persistent);

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

std::shared_ptr<DescriptorSet> VulkanDescriptorManager::allocate_bindless(std::span<DescriptorItem> layout)
{
    const VkDescriptorSetLayout descriptor_set_layout =
        get_descriptor_set_layout(layout, DescriptorSetAllocationType::Bindless);

    // TODO: Careful with this, currently forcing bindless layouts to only have one descriptor
    const uint32_t max_binding = layout[0].count - 1;

    VkDescriptorSetVariableDescriptorCountAllocateInfo descriptor_count_allocate_info{};
    descriptor_count_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
    descriptor_count_allocate_info.descriptorSetCount = 1;
    descriptor_count_allocate_info.pDescriptorCounts = &max_binding;

    VkDescriptorSetAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocate_info.pNext = &descriptor_count_allocate_info;
    allocate_info.descriptorPool = m_bindless_descriptor_pool;
    allocate_info.descriptorSetCount = 1;
    allocate_info.pSetLayouts = &descriptor_set_layout;

    VkDescriptorSet descriptor_set;
    VK_CHECK(vkAllocateDescriptorSets(VulkanContext.device->handle(), &allocate_info, &descriptor_set));

    return std::make_shared<VulkanDescriptorSet>(descriptor_set, *this, DescriptorSetAllocationType::Bindless);
}

VkDescriptorSetLayout VulkanDescriptorManager::get_descriptor_set_layout(
    std::span<DescriptorItem> layout,
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
        binding.binding = item.binding;
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
    m_tracked_transient_resources.insert(descriptor_set);
}

void VulkanDescriptorManager::transient_descriptor_set_freed(VulkanDescriptorSet* descriptor_set)
{
    MIZU_ASSERT(
        m_tracked_transient_resources.contains(descriptor_set),
        "Trying to free descriptor set that is not tracked with address '{}'",
        reinterpret_cast<uintptr_t>(descriptor_set));

    m_tracked_transient_resources.erase(descriptor_set);
}

#endif

} // namespace Mizu::Vulkan