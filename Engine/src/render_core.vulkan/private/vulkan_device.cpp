#include "vulkan_device.h"

#include <span>
#include <unordered_set>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "mizu_render_core_vulkan_module.h"
#include "vulkan_acceleration_structure.h"
#include "vulkan_buffer_resource.h"
#include "vulkan_command_buffer.h"
#include "vulkan_context.h"
#include "vulkan_framebuffer.h"
#include "vulkan_image_resource.h"
#include "vulkan_pipeline.h"
#include "vulkan_queue.h"
#include "vulkan_resource_group.h"
#include "vulkan_sampler_state.h"
#include "vulkan_shader.h"
#include "vulkan_swapchain.h"
#include "vulkan_synchronization.h"

namespace Mizu::Vulkan
{

[[maybe_unused]] static bool is_instance_extension_available(const char* extension)
{
    uint32_t count;
    vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &count, VK_NULL_HANDLE);

    std::vector<VkExtensionProperties> extensions{count};
    vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &count, extensions.data());

    for (const VkExtensionProperties& ext : extensions)
    {
        if (strcmp(ext.extensionName, extension) == 0)
        {
            return true;
        }
    }

    return false;
}

[[maybe_unused]] static bool is_instance_layer_available(const char* layer)
{
    uint32_t layerCount;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

    std::vector<VkLayerProperties> layers(layerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, layers.data()));

    for (const VkLayerProperties& lay : layers)
    {
        if (strcmp(lay.layerName, layer) == 0)
        {
            return true;
        }
    }

    return false;
}

static VkPhysicalDeviceProperties get_physical_device_properties(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);

    return properties;
}

static VkPhysicalDeviceFeatures get_physical_device_features(VkPhysicalDevice device)
{
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    return features;
}

static bool is_physical_device_extension_available(VkPhysicalDevice device, const char* extension)
{
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);

    std::vector<VkExtensionProperties> device_extension_properties{count};
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, device_extension_properties.data());

    for (const VkExtensionProperties& properties : device_extension_properties)
    {
        if (strcmp(properties.extensionName, extension) == 0)
            return true;
    }

    return false;
}

static std::vector<VkQueueFamilyProperties> get_queue_family_properties(VkPhysicalDevice device)
{
    uint32_t count;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_family_properties{count};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queue_family_properties.data());

    return queue_family_properties;
}

VulkanDevice::VulkanDevice(const DeviceCreationDescription& desc)
{
    MIZU_ASSERT(
        std::holds_alternative<VulkanSpecificConfiguration>(desc.specific_config),
        "specific_config is not VulkanSpecificConfiguration");

    const VulkanSpecificConfiguration& vulkan_config = std::get<VulkanSpecificConfiguration>(desc.specific_config);

    constexpr size_t MAX_INSTANCE_EXTENSIONS = 10;
    inplace_vector<const char*, MAX_INSTANCE_EXTENSIONS> instance_extensions;

    for (const char* extension : vulkan_config.instance_extensions)
    {
        MIZU_ASSERT(
            is_instance_extension_available(extension),
            "Trying to request unavailable instance extension: {}",
            extension);
        instance_extensions.push_back(extension);
    }

#if MIZU_VULKAN_VALIDATIONS_ENABLED
    // Enable Vulkan debug utils extension if available
    const bool debug_extension_enabled = is_instance_extension_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#else
    const bool debug_extension_enabled = false;
#endif

    if (debug_extension_enabled)
    {
        instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    create_instance(desc, instance_extensions);

    if (debug_extension_enabled)
    {
        VK_DEBUG_INIT(m_instance);
    }

    select_physical_device();
    create_device(instance_extensions);

    VulkanContext.device = this;

    VulkanContext.pipeline_layout_cache = std::make_unique<VulkanPipelineLayoutCache>();
    VulkanContext.layout_cache = std::make_unique<VulkanDescriptorLayoutCache>();

    VulkanDescriptorPool::PoolSize pool_size = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_SAMPLER, 5},
    };

    if (m_properties.ray_tracing_hardware)
        pool_size.emplace_back(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 5);

    VulkanContext.descriptor_pool = std::make_unique<VulkanDescriptorPool>(pool_size, 100, true);
    VulkanContext.default_device_allocator = std::make_unique<VulkanBaseDeviceMemoryAllocator>();

    if (m_properties.ray_tracing_hardware)
    {
        initialize_rtx(m_device, m_physical_device);
    }
}

VulkanDevice::~VulkanDevice()
{
    // NOTE: Order of destruction matters

    VulkanContext.default_device_allocator.reset();
    VulkanContext.descriptor_pool.reset();
    VulkanContext.layout_cache.reset();
    VulkanContext.pipeline_layout_cache.reset();

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
    vkDestroyInstance(m_instance, nullptr);
}

void VulkanDevice::wait_idle() const
{
    std::lock_guard lock(m_device_mutex);
    VK_CHECK(vkDeviceWaitIdle(m_device));
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
    std::span command_buffers{&command_buffer, 1};
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

std::shared_ptr<VulkanQueue> VulkanDevice::get_graphics_queue() const
{
    MIZU_ASSERT(m_graphics_queue != nullptr, "No graphics queue created");
    return m_graphics_queue;
}

std::shared_ptr<VulkanQueue> VulkanDevice::get_compute_queue() const
{
    MIZU_ASSERT(m_compute_queue != nullptr, "No compute queue created");
    return m_compute_queue;
}

std::shared_ptr<VulkanQueue> VulkanDevice::get_transfer_queue() const
{
    MIZU_ASSERT(m_transfer_queue != nullptr, "No transfer queue created");
    return m_transfer_queue;
}

void VulkanDevice::create_instance(const DeviceCreationDescription& desc, std::span<const char*> extensions)
{
    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = desc.application_name.data();
    application_info.applicationVersion =
        VK_MAKE_VERSION(desc.application_version.major, desc.application_version.minor, desc.application_version.patch),
    application_info.pEngineName = desc.engine_name.data();
    application_info.engineVersion =
        VK_MAKE_VERSION(desc.engine_version.major, desc.engine_version.minor, desc.engine_version.patch),

#if defined(VK_API_VERSION_1_4)
    application_info.apiVersion = VK_API_VERSION_1_4;
#elif defined(VK_API_VERSION_1_3)
    application_info.apiVersion = VK_API_VERSION_1_3;
#else
#error Vulkan sdk does not have a supported api version, either VK_API_VERSION_1_4 or VK_API_VERSION_1_3 must be available.
#endif

    inplace_vector<const char*, 5> layers;
#if MIZU_VULKAN_VALIDATIONS_ENABLED
    layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

#if MIZU_VULKAN_VALIDATIONS_ENABLED
    for (const char* layer : layers)
    {
        MIZU_ASSERT(is_instance_layer_available(layer), "Instance layer is not available: {}", layer);
    }
#endif

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &application_info;
    create_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    create_info.ppEnabledLayerNames = layers.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    VK_CHECK(vkCreateInstance(&create_info, nullptr, &m_instance));
}

void VulkanDevice::select_physical_device()
{
    uint32_t physical_devices_count;
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &physical_devices_count, nullptr));

    std::vector<VkPhysicalDevice> physical_devices{physical_devices_count};
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &physical_devices_count, physical_devices.data()));

    int32_t biggest_score = std::numeric_limits<int32_t>::min();
    VkPhysicalDevice best_device = VK_NULL_HANDLE;
    QueueFamilies best_queue_families{};

    // Notes on Physical Device selection:
    // - Prefer discrete GPUs
    // - Prefer if specific queue types have their own queue (e.g. graphics queue different from compute queue)

    constexpr uint32_t DISCRETE_GPU_SCORE = 40;
    constexpr uint32_t SPECIFIC_QUEUE_TYPE_SCORE = 10;

    for (const VkPhysicalDevice& device : physical_devices)
    {
        int32_t score = 0;

        QueueFamilies queue_families{};

        bool specific_graphics_queue = false;
        bool specific_compute_queue = false;
        bool specific_transfer_queue = false;

        const VkPhysicalDeviceProperties& properties = get_physical_device_properties(device);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score += DISCRETE_GPU_SCORE;

        const std::vector<VkQueueFamilyProperties>& queue_properties = get_queue_family_properties(device);
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

    // Retrieve physical device properties
    const VkPhysicalDeviceProperties& properties = get_physical_device_properties(m_physical_device);
    const VkPhysicalDeviceFeatures& features = get_physical_device_features(m_physical_device);

    m_properties = {};
    m_properties.name = properties.deviceName;
    m_properties.depth_clamp_enabled = features.depthClamp;
    m_properties.async_compute = m_queue_families.compute != m_queue_families.graphics;
    m_properties.ray_tracing_hardware =
        is_physical_device_extension_available(m_physical_device, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
        && is_physical_device_extension_available(m_physical_device, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
        && is_physical_device_extension_available(m_physical_device, VK_KHR_RAY_QUERY_EXTENSION_NAME);
}

class VulkanDeviceFeaturesManager
{
  public:
    VulkanDeviceFeaturesManager(VkPhysicalDevice physical_device) : m_physical_device(physical_device) {}

    void add_extension(const char* extension_name)
    {
        MIZU_ASSERT(
            is_physical_device_extension_available(m_physical_device, extension_name),
            "Extension with name '{}' is not available",
            extension_name);

        m_device_extensions.push_back(extension_name);
    }

    std::span<const char* const> get_device_extensions() const
    {
        return std::span(m_device_extensions.data(), m_device_extensions.size());
    }

    template <typename T>
    T& add(T value = {})
    {
        m_feature_structures.emplace_back(std::make_shared<Container<T>>(value));

        const auto& container = std::static_pointer_cast<Container<T>>(m_feature_structures.back());
        return container->m_value;
    }

    void* build_features()
    {
        if (m_feature_structures.empty())
            return nullptr;

        if (m_feature_structures.size() == 1)
            return m_feature_structures[0]->get_address();

        for (size_t i = m_feature_structures.size() - 1; i > 0; --i)
        {
            auto& current = m_feature_structures[i];
            auto& next = m_feature_structures[i - 1];
            next->set_pnext(current->get_address());
        }

        return m_feature_structures[0]->get_address();
    }

  private:
    struct IContainer
    {
        virtual ~IContainer() = default;

        virtual void set_pnext(void* pnext) = 0;
        virtual void* get_address() const = 0;
    };

    template <typename T>
    struct Container : public IContainer
    {
        Container(T value = {}) : m_value(value) {}
        ~Container() override = default;

        void set_pnext(void* pnext) override { m_value.pNext = pnext; }
        void* get_address() const override { return (void*)&m_value; }

        T m_value;
    };

    VkPhysicalDevice m_physical_device;
    std::vector<std::shared_ptr<IContainer>> m_feature_structures;
    std::vector<const char*> m_device_extensions;
};

void VulkanDevice::create_device(std::span<const char*> instance_extensions)
{
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

    VulkanDeviceFeaturesManager device_features{m_physical_device};

    for (const char* extension : instance_extensions)
    {
        // https://vulkan.lunarg.com/doc/view/1.3.275.0/linux/1.3-extensions/vkspec.html#VK_KHR_swapchain
        if (strcmp(extension, VK_KHR_SURFACE_EXTENSION_NAME) == 0)
        {
            device_features.add_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }
    }

    device_features.add_extension(VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME);
    device_features.add_extension(VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME);

    device_features.add_extension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    auto& descriptor_indexing_features = device_features.add<VkPhysicalDeviceDescriptorIndexingFeatures>();
    descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

    if (m_properties.ray_tracing_hardware)
    {
        device_features.add_extension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
        device_features.add_extension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        device_features.add_extension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        device_features.add_extension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
        device_features.add_extension(VK_KHR_RAY_QUERY_EXTENSION_NAME);

        auto& buffer_device_address_features = device_features.add<VkPhysicalDeviceBufferDeviceAddressFeaturesKHR>();
        buffer_device_address_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
        buffer_device_address_features.bufferDeviceAddress = VK_TRUE;

        auto& acceleration_structure_features = device_features.add<VkPhysicalDeviceAccelerationStructureFeaturesKHR>();
        acceleration_structure_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        acceleration_structure_features.accelerationStructure = VK_TRUE;

        auto& ray_tracing_pipeline_features = device_features.add<VkPhysicalDeviceRayTracingPipelineFeaturesKHR>();
        ray_tracing_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        ray_tracing_pipeline_features.rayTracingPipeline = VK_TRUE;
    }

    std::span<const char* const> device_extensions = device_features.get_device_extensions();
    void* create_info_pnext = device_features.build_features();

    VkPhysicalDeviceFeatures2 physical_device_features2{};
    physical_device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    physical_device_features2.pNext = create_info_pnext;

    vkGetPhysicalDeviceFeatures2(m_physical_device, &physical_device_features2);

    // Create device
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &physical_device_features2;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();
    create_info.pEnabledFeatures = nullptr;

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

//
// Creation functions
//

std::shared_ptr<BufferResource> VulkanDevice::create_buffer(const BufferDescription& desc) const
{
    return std::make_shared<VulkanBufferResource>(desc);
}
std::shared_ptr<ImageResource> VulkanDevice::create_image(const ImageDescription& desc) const
{
    return std::make_shared<VulkanImageResource>(desc);
}

std::shared_ptr<AccelerationStructure> VulkanDevice::create_acceleration_structure(
    const AccelerationStructureDescription& desc) const
{
    return std::make_shared<VulkanAccelerationStructure>(desc);
}

std::shared_ptr<CommandBuffer> VulkanDevice::create_command_buffer(CommandBufferType type) const
{
    return std::make_shared<VulkanCommandBuffer>(type);
}

std::shared_ptr<Framebuffer> VulkanDevice::create_framebuffer(const FramebufferDescription& desc) const
{
    return std::make_shared<VulkanFramebuffer>(desc);
}

std::shared_ptr<Shader> VulkanDevice::create_shader(const ShaderDescription& desc) const
{
    return std::make_shared<VulkanShader>(desc);
}

std::shared_ptr<SamplerState> VulkanDevice::create_sampler_state(const SamplerStateDescription& desc) const
{
    return std::make_shared<VulkanSamplerState>(desc);
}

std::shared_ptr<Pipeline> VulkanDevice::create_pipeline(const GraphicsPipelineDescription& desc) const
{
    return std::make_shared<VulkanPipeline>(desc);
}

std::shared_ptr<Pipeline> VulkanDevice::create_pipeline(const ComputePipelineDescription& desc) const
{
    return std::make_shared<VulkanPipeline>(desc);
}

std::shared_ptr<Pipeline> VulkanDevice::create_pipeline(const RayTracingPipelineDescription& desc) const
{
    return std::make_shared<VulkanPipeline>(desc);
}

std::shared_ptr<ResourceGroup> VulkanDevice::create_resource_group(const ResourceGroupBuilder& builder) const
{
    return std::make_shared<VulkanResourceGroup>(builder);
}

std::shared_ptr<Semaphore> VulkanDevice::create_semaphore() const
{
    return std::make_shared<VulkanSemaphore>();
}

std::shared_ptr<Fence> VulkanDevice::create_fence(bool signaled) const
{
    return std::make_shared<VulkanFence>(signaled);
}

std::shared_ptr<Swapchain> VulkanDevice::create_swapchain(const SwapchainDescription& desc) const
{
    return std::make_shared<VulkanSwapchain>(desc);
}

std::shared_ptr<AliasedDeviceMemoryAllocator> VulkanDevice::create_aliased_memory_allocator(
    bool host_visible,
    std::string name) const
{
    return std::make_shared<VulkanAliasedDeviceMemoryAllocator>(host_visible, name);
}

} // namespace Mizu::Vulkan

extern "C" MIZU_RENDER_CORE_VULKAN_API Mizu::Device* create_rhi_device(const Mizu::DeviceCreationDescription& desc)
{
    return new Mizu::Vulkan::VulkanDevice{desc};
}