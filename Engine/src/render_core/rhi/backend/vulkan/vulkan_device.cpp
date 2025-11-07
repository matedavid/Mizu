#include "vulkan_device.h"

#include <array>
#include <cstring>
#include <numeric>
#include <unordered_set>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "render_core/rhi/backend/vulkan/vulkan_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_instance.h"
#include "render_core/rhi/backend/vulkan/vulkan_queue.h"

namespace Mizu::Vulkan
{

VulkanDevice::VulkanDevice(const VulkanInstance& instance, const std::vector<const char*>& instance_extensions)
{
    // Select physical device
    select_physical_device(instance);
    MIZU_ASSERT(m_physical_device != VK_NULL_HANDLE, "No suitable VkPhysicalDevice found");

    retrieve_physical_device_capabilities();

    MIZU_LOG_INFO("Selected device: {}", get_physical_device_properties(m_physical_device).deviceName);

    // Create queues
    VkDeviceQueueCreateInfo queue_create_info_base{};
    queue_create_info_base.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info_base.queueCount = 1;
    float priority = 1.0f;
    queue_create_info_base.pQueuePriorities = &priority;

    std::unordered_map<uint32_t, VkDeviceQueueCreateInfo> queue_family_to_create_info;
    {
        queue_create_info_base.queueFamilyIndex = m_queue_families.graphics;
        queue_family_to_create_info[m_queue_families.graphics] = queue_create_info_base;

        queue_create_info_base.queueFamilyIndex = m_queue_families.compute;
        queue_family_to_create_info[m_queue_families.compute] = queue_create_info_base;

        queue_create_info_base.queueFamilyIndex = m_queue_families.transfer;
        queue_family_to_create_info[m_queue_families.transfer] = queue_create_info_base;
    }

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    for (const auto& [_, queue_create_info] : queue_family_to_create_info)
    {
        queue_create_infos.push_back(queue_create_info);
    }

    // Populate extension dependencies
    std::vector<const char*> device_extensions;
    for (const char* extension : instance_extensions)
    {
        // https://vulkan.lunarg.com/doc/view/1.3.275.0/linux/1.3-extensions/vkspec.html#VK_KHR_swapchain
        if (strcmp(extension, VK_KHR_SURFACE_EXTENSION_NAME) == 0)
            device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    // TODO: This should not be here, but to make my life easier I will force the swapchain mutable format extension to
    // be available
    MIZU_ASSERT(
        has_extension(VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME),
        "Device does not support swapchain mutable format extension");
    device_extensions.push_back(VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME);

    void* create_info_p_next = nullptr;

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR buffer_device_address_features{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features{};

    if (m_capabilities.ray_tracing_hardware)
    {
        device_extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        device_extensions.push_back(
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME); // Required by VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME
        device_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        device_extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);

        buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
        buffer_device_address_features.bufferDeviceAddress = VK_TRUE;

        acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        acceleration_structure_features.accelerationStructure = VK_TRUE;

        ray_tracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        ray_tracing_pipeline_features.rayTracingPipeline = VK_TRUE;

        create_info_p_next = &buffer_device_address_features;
        buffer_device_address_features.pNext = &acceleration_structure_features;
        acceleration_structure_features.pNext = &ray_tracing_pipeline_features;
        ray_tracing_pipeline_features.pNext = nullptr;
    }

    const VkPhysicalDeviceFeatures& features = get_physical_device_features(m_physical_device);

    // Create device
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = create_info_p_next;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();
    create_info.pEnabledFeatures = &features;

    VK_CHECK(vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device));

    // Retrieve queues
    std::unordered_map<uint32_t, std::shared_ptr<VulkanQueue>> queue_family_to_queue;
    auto get_queue_or_insert = [&](uint32_t idx) -> std::shared_ptr<VulkanQueue> {
        const auto it = queue_family_to_queue.find(idx);
        if (it == queue_family_to_queue.end())
        {
            VkQueue queue;
            vkGetDeviceQueue(m_device, idx, 0, &queue);

            auto q = std::make_shared<VulkanQueue>(queue, idx);
            queue_family_to_queue.insert({idx, q});
            return q;
        }

        return it->second;
    };

    m_graphics_queue = get_queue_or_insert(m_queue_families.graphics);
    m_compute_queue = get_queue_or_insert(m_queue_families.compute);
    m_transfer_queue = get_queue_or_insert(m_queue_families.transfer);

    // Create command pools
    const uint32_t num_threads = std::thread::hardware_concurrency();
    m_per_thread_command_info.reserve(num_threads);

    for (uint32_t i = 0; i < num_threads; ++i)
    {
        m_per_thread_command_info.push_back(create_thread_command_info());
        m_available_per_thread_command_info_idx.push(num_threads - i - 1); // So that the idx on top of the stack is 0
    }
}

VulkanDevice::~VulkanDevice()
{
    // Doing this strange stuff to prevent the same command pool from being destroyed twice, if two
    // "command pool types" use the same queue.
    std::unordered_set<VkCommandPool> command_pools;
    for (const ThreadCommandInfo& info : m_per_thread_command_info)
    {
        command_pools.insert(info.graphics.command_pool);
        command_pools.insert(info.compute.command_pool);
        command_pools.insert(info.transfer.command_pool);
    }

    for (const VkCommandPool& cp : command_pools)
    {
        vkDestroyCommandPool(m_device, cp, nullptr);
    }

    vkDestroyDevice(m_device, nullptr);
}

std::shared_ptr<VulkanQueue> VulkanDevice::get_graphics_queue() const
{
    MIZU_ASSERT(m_graphics_queue != nullptr, "Graphics functionality not requested");
    return m_graphics_queue;
}

std::shared_ptr<VulkanQueue> VulkanDevice::get_compute_queue() const
{
    MIZU_ASSERT(m_compute_queue != nullptr, "Compute functionality not requested");
    return m_compute_queue;
}

std::shared_ptr<VulkanQueue> VulkanDevice::get_transfer_queue() const
{
    return m_transfer_queue;
}

VkCommandBuffer VulkanDevice::allocate_command_buffer(CommandBufferType type)
{
    return allocate_command_buffers(1, type)[0];
}

std::vector<VkCommandBuffer> VulkanDevice::allocate_command_buffers(uint32_t count, CommandBufferType type)
{
    ThreadCommandInfo::Type& thread_info = get_thread_command_info(std::this_thread::get_id(), type);
    MIZU_ASSERT(thread_info.command_pool != VK_NULL_HANDLE, "Could not select command pool");

    if (thread_info.available_command_buffers.size() >= count)
    {
        std::vector<VkCommandBuffer> command_buffers(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            command_buffers[i] = thread_info.available_command_buffers.top();
            thread_info.available_command_buffers.pop();
            thread_info.command_buffers_in_usage++;
        }

        return command_buffers;
    }

    VkCommandBufferAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = count;
    allocate_info.commandPool = thread_info.command_pool;

    std::vector<VkCommandBuffer> command_buffers{count};
    VK_CHECK(vkAllocateCommandBuffers(m_device, &allocate_info, command_buffers.data()));

    thread_info.command_buffers_in_usage += count;

    return command_buffers;
}

void VulkanDevice::free_command_buffer(VkCommandBuffer command_buffer, CommandBufferType type)
{
    std::span<VkCommandBuffer> command_buffers{&command_buffer, 1};
    free_command_buffers(command_buffers, type);
}

void VulkanDevice::free_command_buffers(std::span<VkCommandBuffer> command_buffers, CommandBufferType type)
{
    const std::thread::id thread_id = std::this_thread::get_id();

    ThreadCommandInfo::Type& thread_info = get_thread_command_info(thread_id, type);
    MIZU_ASSERT(thread_info.command_pool != VK_NULL_HANDLE, "Could not select command pool");

    for (const VkCommandBuffer& command_buffer : command_buffers)
    {
        thread_info.available_command_buffers.push(command_buffer);
        thread_info.command_buffers_in_usage--;
    }

    if (thread_info.command_buffers_in_usage == 0)
    {
        const std::lock_guard lock(m_assign_thread_info_mutex);

        // If the number of command buffers in usage in a specific thread is 0, release this Thread Command info
        // structure to be used by another thread
        const auto it = m_thread_to_command_info_map.find(thread_id);
        m_available_per_thread_command_info_idx.push(it->second);
        m_thread_to_command_info_map.erase(it);
    }
}

std::optional<uint32_t> VulkanDevice::find_memory_type(uint32_t filter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i)
    {
        if (filter & (1 << i) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    return {};
}

bool VulkanDevice::has_extension(const char* extension) const
{
    const std::vector<VkExtensionProperties>& extensions = get_physical_device_extension_properties(m_physical_device);

    for (const VkExtensionProperties& properties : extensions)
    {
        if (strcmp(properties.extensionName, extension) == 0)
            return true;
    }

    return false;
}

void VulkanDevice::select_physical_device(const VulkanInstance& instance)
{
    const auto devices = instance.get_physical_devices();

    int32_t biggest_score = std::numeric_limits<int32_t>::min();
    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    QueueFamilies best_queue_families{};

    // Notes on Physical Device selection:
    // - Prefer discrete GPUs
    // - Prefer if specific queue types have their own queue (e.g. graphics queue different from compute queue)

    constexpr uint32_t DISCRETE_GPU_SCORE = 40;
    constexpr uint32_t SPECIFIC_QUEUE_TYPE_SCORE = 10;

    for (const VkPhysicalDevice& device : devices)
    {
        int32_t score = 0;

        QueueFamilies queue_families{};

        bool specific_graphics_queue = false;
        bool specific_compute_queue = false;
        bool specific_transfer_queue = false;

        const VkPhysicalDeviceProperties& properties = get_physical_device_properties(device);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score += DISCRETE_GPU_SCORE;

        const auto& queue_properties = get_queue_family_properties(device);
        for (uint32_t idx = 0; idx < queue_properties.size(); ++idx)
        {
            const bool has_graphics = queue_properties[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT;
            const bool has_compute = queue_properties[idx].queueFlags & VK_QUEUE_COMPUTE_BIT;
            const bool has_transfer = queue_properties[idx].queueFlags & VK_QUEUE_TRANSFER_BIT;

            // Has queue with only graphics operation
            if (has_graphics && !has_compute && !specific_graphics_queue)
            {
                score += SPECIFIC_QUEUE_TYPE_SCORE;

                queue_families.graphics = idx;
                specific_graphics_queue = true;
            }
            else if (has_graphics && !specific_graphics_queue)
            {
                queue_families.graphics = idx;
            }

            // Has queue with only compute operation
            if (has_compute && !has_graphics && !specific_compute_queue)
            {
                score += SPECIFIC_QUEUE_TYPE_SCORE;

                queue_families.compute = idx;
                specific_compute_queue = true;
            }
            else if (has_compute && !specific_compute_queue)
            {
                queue_families.compute = idx;
            }

            // Has queue with only transfer operation
            if (has_transfer && !has_graphics && !has_compute && !specific_transfer_queue)
            {
                score += SPECIFIC_QUEUE_TYPE_SCORE;

                queue_families.transfer = idx;
                specific_transfer_queue = true;
            }
            else if (has_transfer && !specific_transfer_queue)
            {
                queue_families.transfer = idx;
            }
        }

        if (score > biggest_score)
        {
            best_device = device;
            biggest_score = score;
            best_queue_families = queue_families;
        }
    }

    m_physical_device = best_device;
    m_queue_families = best_queue_families;
}

void VulkanDevice::retrieve_physical_device_capabilities()
{
    MIZU_ASSERT(m_physical_device != VK_NULL_HANDLE, "No physical device selected");

    const VkPhysicalDeviceProperties& properties = get_physical_device_properties(m_physical_device);
    const VkPhysicalDeviceFeatures& features = get_physical_device_features(m_physical_device);

    m_capabilities = {};
    m_capabilities.max_resource_group_sets = properties.limits.maxBoundDescriptorSets;
    m_capabilities.max_push_constant_size = properties.limits.maxPushConstantsSize;
    m_capabilities.depth_clamp_enabled = features.depthClamp;
    m_capabilities.async_compute = m_queue_families.compute != m_queue_families.graphics;
    m_capabilities.ray_tracing_hardware = has_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
                                          && has_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
                                          && has_extension(VK_KHR_RAY_QUERY_EXTENSION_NAME);
}

std::vector<VkExtensionProperties> VulkanDevice::get_physical_device_extension_properties(
    VkPhysicalDevice physical_device)
{
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, nullptr);

    std::vector<VkExtensionProperties> device_extension_properties(count);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &count, device_extension_properties.data());

    return device_extension_properties;
}

VkPhysicalDeviceProperties VulkanDevice::get_physical_device_properties(VkPhysicalDevice physical_device)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);

    return properties;
}

VkPhysicalDeviceFeatures VulkanDevice::get_physical_device_features(VkPhysicalDevice physical_device)
{
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physical_device, &features);

    return features;
}

std::vector<VkQueueFamilyProperties> VulkanDevice::get_queue_family_properties(VkPhysicalDevice physical_device)
{
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_family_properties{count};
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, queue_family_properties.data());

    return queue_family_properties;
}

VulkanDevice::ThreadCommandInfo VulkanDevice::create_thread_command_info()
{
    VkCommandPoolCreateInfo command_pool_create_info{};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    std::unordered_map<uint32_t, VkCommandPool> queue_family_to_command_pool;
    const auto get_command_pool_or_insert = [&](uint32_t idx) -> VkCommandPool {
        const auto it = queue_family_to_command_pool.find(idx);
        if (it == queue_family_to_command_pool.end())
        {
            VkCommandPool cp;

            command_pool_create_info.queueFamilyIndex = idx;
            VK_CHECK(vkCreateCommandPool(m_device, &command_pool_create_info, nullptr, &cp));

            queue_family_to_command_pool.insert({idx, cp});
            return cp;
        }

        return it->second;
    };

    const auto create_type = [&](uint32_t queue_family) -> ThreadCommandInfo::Type {
        ThreadCommandInfo::Type command_info{};
        command_info.command_pool = get_command_pool_or_insert(queue_family);
        command_info.command_buffers_in_usage = 0;

        constexpr uint32_t STARTING_COMMAND_BUFFER_COUNT = 5;

        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = command_info.command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = STARTING_COMMAND_BUFFER_COUNT;

        std::array<VkCommandBuffer, STARTING_COMMAND_BUFFER_COUNT> command_buffers{};
        VK_CHECK(vkAllocateCommandBuffers(m_device, &allocate_info, command_buffers.data()));

        for (VkCommandBuffer& command_buffer : command_buffers)
        {
            command_info.available_command_buffers.push(command_buffer);
        }

        return command_info;
    };

    ThreadCommandInfo info{};
    info.graphics = create_type(m_queue_families.graphics);
    info.compute = create_type(m_queue_families.compute);
    info.transfer = create_type(m_queue_families.transfer);

    return info;
}

VulkanDevice::ThreadCommandInfo::Type& VulkanDevice::get_thread_command_info(std::thread::id id, CommandBufferType type)
{
    auto it = m_thread_to_command_info_map.find(id);
    if (it == m_thread_to_command_info_map.end())
    {
        const std::lock_guard lock(m_assign_thread_info_mutex);

        if (m_available_per_thread_command_info_idx.empty())
        {
            m_per_thread_command_info.push_back(create_thread_command_info());
            m_available_per_thread_command_info_idx.push(static_cast<uint32_t>(m_per_thread_command_info.size() - 1));
        }

        const uint32_t next_idx = m_available_per_thread_command_info_idx.top();
        m_available_per_thread_command_info_idx.pop();

        it = m_thread_to_command_info_map.insert({id, next_idx}).first;
    }

    ThreadCommandInfo& info = m_per_thread_command_info[it->second];
    return info.get_type(type);
}

} // namespace Mizu::Vulkan