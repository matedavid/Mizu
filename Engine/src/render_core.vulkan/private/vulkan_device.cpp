#include "vulkan_device.h"

#include <array>
#include <cstring>
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
#include "vulkan_descriptors.h"
#include "vulkan_gpu_heap.h"
#include "vulkan_image_resource.h"
#include "vulkan_pipeline.h"
#include "vulkan_queue.h"
#include "vulkan_sampler_state.h"
#include "vulkan_shader.h"
#include "vulkan_swapchain.h"
#include "vulkan_synchronization.h"

namespace Mizu::Vulkan
{

static constexpr uint32_t NUM_BINDLESS_DESCRIPTORS = 400'000;

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

    VulkanContext.binding_offsets = vulkan_config.binding_offsets;

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
    VulkanContext.validations_enabled =
        desc.validations_enabled && is_instance_extension_available(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    if (VulkanContext.validations_enabled)
    {
        instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#else
    VulkanContext.validations_enabled = false;
#endif

    create_instance(desc, instance_extensions);

#if MIZU_VULKAN_VALIDATIONS_ENABLED
    if (VulkanContext.validations_enabled)
    {
        VK_DEBUG_INIT(m_instance);
    }
#endif

    DeviceRequirements requirements{};
    for (const char* extension : instance_extensions)
    {
        if (strcmp(extension, VK_KHR_SURFACE_EXTENSION_NAME) == 0)
        {
            requirements.swapchain = true;
        }
    }

    select_physical_device(requirements);
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_memory_properties);

    VulkanContext.device = this;
    create_device(requirements);

    VulkanContext.descriptor_set_layout_cache = std::make_unique<VulkanDescriptorSetLayoutCache>();
    VulkanContext.pipeline_layout_cache = std::make_unique<VulkanPipelineLayoutCache>();

    inplace_vector<VkDescriptorPoolSize, 10> transient_persistent_pool_sizes = {
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = 500},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 250},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 100},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 100},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 50},
    };

    if (m_properties.ray_tracing.supported)
    {
        transient_persistent_pool_sizes.push_back(
            VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, .descriptorCount = 50});
    }

    std::array bindless_pool_sizes = {
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = m_properties.max_bindless_descriptors,
        },
    };

    VulkanDescriptorManagerDescription descriptor_manager_desc{};
    descriptor_manager_desc.transient_pool_sizes = transient_persistent_pool_sizes;
    descriptor_manager_desc.persistent_pool_sizes = transient_persistent_pool_sizes;
    descriptor_manager_desc.bindless_pool_sizes = bindless_pool_sizes;
    descriptor_manager_desc.num_transient_pools = desc.frames_in_flight;
    VulkanContext.descriptor_manager = std::make_unique<VulkanDescriptorManager>(descriptor_manager_desc);

    if (m_properties.ray_tracing.supported)
    {
        initialize_rtx(m_device, m_physical_device);
    }
}

VulkanDevice::~VulkanDevice()
{
    // NOTE: Order of destruction matters

    wait_idle();
    destroy_default_allocator();

    VulkanContext.pipeline_layout_cache.reset();
    VulkanContext.descriptor_set_layout_cache.reset();
    VulkanContext.descriptor_manager.reset();

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

    m_graphics_queue = nullptr;
    m_compute_queue = nullptr;
    m_transfer_queue = nullptr;
    m_queues.clear();

    vkDestroyDevice(m_device, nullptr);
    vkDestroyInstance(m_instance, nullptr);
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

std::optional<uint32_t> VulkanDevice::find_memory_type_index(HeapDomain domain, uint32_t memory_type_bits) const
{
    // These memory types can only back special resources, not general usage heaps
    // - LAZILY_ALLOCATED: can only be bound to images with VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT, the
    //   implementation may commit the memory lazily (or never, e.g. tile memory on mobile GPUs).
    // - PROTECTED: can only be bound to protected resources and requires the protectedMemory feature.
    // https://docs.vulkan.org/refpages/latest/refpages/source/VkMemoryPropertyFlagBits.html

    constexpr VkMemoryPropertyFlags excluded =
        VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_PROTECTED_BIT;

    const auto find_best = [&](VkMemoryPropertyFlags required, VkMemoryPropertyFlags avoided) {
        std::optional<uint32_t> best;
        int32_t best_score = std::numeric_limits<int32_t>::max();

        for (uint32_t i = 0; i < m_memory_properties.memoryTypeCount; ++i)
        {
            const VkMemoryPropertyFlags flags = m_memory_properties.memoryTypes[i].propertyFlags;

            if (!(memory_type_bits & (1u << i)))
                continue;

            if ((flags & required) != required)
                continue;

            if (flags & excluded)
                continue;

            const int32_t score = (flags & avoided) ? 1 : 0;
            if (score < best_score)
            {
                best = i;
                best_score = score;
            }
        }

        return best;
    };

    switch (domain)
    {
    case HeapDomain::DeviceLocal:
        return find_best(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    case HeapDomain::Upload:
        return find_best(
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    case HeapDomain::Readback: {
        const std::optional<uint32_t> cached = find_best(
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (cached.has_value())
            return cached;

        return find_best(
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }
    }

    return std::nullopt;
}

bool VulkanDevice::is_queue_available(CommandBufferType type) const
{
    switch (type)
    {
    case CommandBufferType::Graphics:
        return true;
    case CommandBufferType::Compute:
        return m_compute_queue != m_graphics_queue;
    case CommandBufferType::Transfer:
        return m_transfer_queue != m_graphics_queue;
    }
}

VulkanQueue& VulkanDevice::get_native_queue(CommandBufferType type) const
{
    switch (type)
    {
    case CommandBufferType::Graphics:
        return *m_graphics_queue;
    case CommandBufferType::Compute:
        return *m_compute_queue;
    case CommandBufferType::Transfer:
        return *m_transfer_queue;
    }

    MIZU_UNREACHABLE("Invalid CommandBufferType");
}

Queue& VulkanDevice::get_queue(CommandBufferType type) const
{
    return get_native_queue(type);
}

void VulkanDevice::create_instance(const DeviceCreationDescription& desc, std::span<const char*> extensions)
{
    VkApplicationInfo application_info{};
    application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application_info.pApplicationName = desc.application_name.data();
    application_info.applicationVersion =
        VK_MAKE_VERSION(desc.application_version.major, desc.application_version.minor, desc.application_version.patch);
    application_info.pEngineName = desc.engine_name.data();
    application_info.engineVersion =
        VK_MAKE_VERSION(desc.engine_version.major, desc.engine_version.minor, desc.engine_version.patch);

#if defined(VK_API_VERSION_1_4)
    application_info.apiVersion = VK_API_VERSION_1_4;
#elif defined(VK_API_VERSION_1_3)
    application_info.apiVersion = VK_API_VERSION_1_3;
#else
#error Vulkan sdk does not have a supported api version, either VK_API_VERSION_1_4 or VK_API_VERSION_1_3 must be available.
#endif

    inplace_vector<const char*, 5> layers;
#if MIZU_VULKAN_VALIDATIONS_ENABLED
    if (VulkanContext.validations_enabled)
    {
        layers.push_back("VK_LAYER_KHRONOS_validation");

        for (const char* layer : layers)
        {
            MIZU_ASSERT(is_instance_layer_available(layer), "Instance layer is not available: {}", layer);
        }
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

template <typename T>
struct VulkanFeatureStructureType;

#define MIZU_VULKAN_FEATURE_STRUCTURE(type, structure_type)      \
    template <>                                                  \
    struct VulkanFeatureStructureType<type>                      \
    {                                                            \
        static constexpr VkStructureType VALUE = structure_type; \
    }

// clang-format off
// Create mapping from struct => sType
MIZU_VULKAN_FEATURE_STRUCTURE(VkPhysicalDeviceFeatures2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
MIZU_VULKAN_FEATURE_STRUCTURE(VkPhysicalDeviceVulkan11Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
MIZU_VULKAN_FEATURE_STRUCTURE(VkPhysicalDeviceVulkan12Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
MIZU_VULKAN_FEATURE_STRUCTURE(VkPhysicalDeviceVulkan13Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
MIZU_VULKAN_FEATURE_STRUCTURE(VkPhysicalDeviceAccelerationStructureFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);
MIZU_VULKAN_FEATURE_STRUCTURE(VkPhysicalDeviceRayTracingPipelineFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR);
MIZU_VULKAN_FEATURE_STRUCTURE(VkPhysicalDeviceRayQueryFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR);
// clang-format on

#undef MIZU_VULKAN_FEATURE_STRUCTURE

class VulkanFeatureChain
{
  public:
    VulkanFeatureChain()
    {
        // This guarantees vkGetPhysicalDeviceFeatures2 is always the first struct in the chain, which is a requirement
        // in VkDeviceCreateInfo.pNext

        add<VkPhysicalDeviceFeatures2>();
    }

    VulkanFeatureChain(const VulkanFeatureChain&) = delete;
    VulkanFeatureChain& operator=(const VulkanFeatureChain&) = delete;

    template <typename T>
    T& get()
    {
        constexpr VkStructureType type = VulkanFeatureStructureType<T>::VALUE;

        for (const std::shared_ptr<void>& structure : m_structures)
        {
            if (static_cast<VkBaseOutStructure*>(structure.get())->sType == type)
                return *static_cast<T*>(structure.get());
        }

        return add<T>();
    }

    VkPhysicalDeviceFeatures2& head() { return get<VkPhysicalDeviceFeatures2>(); }

  private:
    template <typename T>
    T& add()
    {
        const std::shared_ptr<T> structure = std::make_shared<T>();
        structure->sType = VulkanFeatureStructureType<T>::VALUE;

        if (!m_structures.empty())
        {
            VkBaseOutStructure* last = static_cast<VkBaseOutStructure*>(m_structures.back().get());
            last->pNext = reinterpret_cast<VkBaseOutStructure*>(structure.get());
        }

        m_structures.push_back(structure);

        return *structure;
    }

    std::vector<std::shared_ptr<void>> m_structures;
};

struct VulkanFeature
{
    VkBool32& (*get)(VulkanFeatureChain&);
    const char* name;
};

static void add_feature_structures(VulkanFeatureChain& chain, std::span<const VulkanFeature> features)
{
    for (const VulkanFeature& feature : features)
    {
        feature.get(chain);
    }
}

static void enable_features(VulkanFeatureChain& chain, std::span<const VulkanFeature> features)
{
    for (const VulkanFeature& feature : features)
    {
        feature.get(chain) = VK_TRUE;
    }
}

#define MIZU_VULKAN_FEATURE(type, member)                                                        \
    VulkanFeature                                                                                \
    {                                                                                            \
        [](VulkanFeatureChain& chain) -> VkBool32& { return chain.get<type>().member; }, #member \
    }

static const VulkanFeature REQUIRED_FEATURES[] = {
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.multiDrawIndirect),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.shaderInt16),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.shaderInt64),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.shaderSampledImageArrayDynamicIndexing),

    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan11Features, shaderDrawParameters),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan11Features, storageBuffer16BitAccess),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan11Features, uniformAndStorageBuffer16BitAccess),

    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, shaderFloat16),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, drawIndirectCount),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, timelineSemaphore),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorIndexing),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, runtimeDescriptorArray),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, shaderSampledImageArrayNonUniformIndexing),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingVariableDescriptorCount),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingPartiallyBound),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingUpdateUnusedWhilePending),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, descriptorBindingSampledImageUpdateAfterBind),

    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan13Features, synchronization2),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan13Features, dynamicRendering),
};

static const VulkanFeature OPTIONAL_FEATURES[] = {
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.depthClamp),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.depthBiasClamp),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.depthBounds),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.fillModeNonSolid),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.logicOp),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.independentBlend),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.drawIndirectFirstInstance),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.fullDrawIndexUint32),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.imageCubeArray),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.samplerAnisotropy),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.fragmentStoresAndAtomics),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.vertexPipelineStoresAndAtomics),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.shaderStorageImageExtendedFormats),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.shaderStorageImageReadWithoutFormat),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.shaderStorageImageWriteWithoutFormat),

    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan13Features, shaderDemoteToHelperInvocation),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan13Features, maintenance4),
};

static const VulkanFeature SPARSE_BUFFER_FEATURES[] = {
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.sparseBinding),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.sparseResidencyBuffer),
};

static const VulkanFeature SPARSE_IMAGE_FEATURES[] = {
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.sparseBinding),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceFeatures2, features.sparseResidencyImage2D),
};

static const VulkanFeature RAY_TRACING_FEATURES[] = {
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceVulkan12Features, bufferDeviceAddress),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceAccelerationStructureFeaturesKHR, accelerationStructure),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceRayTracingPipelineFeaturesKHR, rayTracingPipeline),
    MIZU_VULKAN_FEATURE(VkPhysicalDeviceRayQueryFeaturesKHR, rayQuery),
};

#undef MIZU_VULKAN_FEATURE

static constexpr const char* SWAPCHAIN_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SWAPCHAIN_MUTABLE_FORMAT_EXTENSION_NAME,
};

static constexpr const char* RAY_TRACING_EXTENSIONS[] = {
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
};

struct VulkanPhysicalDeviceInfo
{
    VkPhysicalDeviceProperties properties{};
    VulkanFeatureChain features{};
    std::vector<VkExtensionProperties> extensions;
    std::vector<VkQueueFamilyProperties> queue_families;
    bool has_ray_tracing_extensions = false;

    bool has_extension(const char* extension) const
    {
        for (const VkExtensionProperties& properties_ext : extensions)
        {
            if (strcmp(properties_ext.extensionName, extension) == 0)
                return true;
        }

        return false;
    }

    bool has_extensions(std::span<const char* const> requested) const
    {
        for (const char* extension : requested)
        {
            if (!has_extension(extension))
                return false;
        }

        return true;
    }
};

static void query_physical_device_info(VkPhysicalDevice device, VulkanPhysicalDeviceInfo& info)
{
    info.properties = get_physical_device_properties(device);
    info.queue_families = get_queue_family_properties(device);

    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    info.extensions.resize(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, info.extensions.data());

    info.has_ray_tracing_extensions = info.has_extensions(RAY_TRACING_EXTENSIONS);

    if (info.properties.apiVersion < VK_API_VERSION_1_3)
        return;

    add_feature_structures(info.features, REQUIRED_FEATURES);
    add_feature_structures(info.features, OPTIONAL_FEATURES);

    if (info.has_ray_tracing_extensions)
    {
        add_feature_structures(info.features, RAY_TRACING_FEATURES);
    }

    vkGetPhysicalDeviceFeatures2(device, &info.features.head());
}

const char* VulkanDevice::find_missing_requirement(
    VulkanPhysicalDeviceInfo& info,
    const DeviceRequirements& requirements)
{
    if (info.properties.apiVersion < VK_API_VERSION_1_3)
        return "Vulkan 1.3";

    if (requirements.swapchain)
    {
        for (const char* extension : SWAPCHAIN_EXTENSIONS)
        {
            if (!info.has_extension(extension))
                return extension;
        }
    }

    for (const VulkanFeature& feature : REQUIRED_FEATURES)
    {
        if (feature.get(info.features) != VK_TRUE)
            return feature.name;
    }

    return nullptr;
}

static bool are_features_supported(VulkanFeatureChain& supported, std::span<const VulkanFeature> features)
{
    for (const VulkanFeature& feature : features)
    {
        if (feature.get(supported) != VK_TRUE)
            return false;
    }

    return true;
}

bool VulkanDevice::select_queue_families(std::span<const VkQueueFamilyProperties> families, QueueFamilies& selection)
{
    constexpr uint32_t INVALID = std::numeric_limits<uint32_t>::max();

    uint32_t graphics = INVALID;
    uint32_t any_compute = INVALID;
    uint32_t dedicated_compute = INVALID;
    uint32_t dedicated_transfer = INVALID;

    for (uint32_t idx = 0; idx < families.size(); ++idx)
    {
        const bool has_graphics = families[idx].queueFlags & VK_QUEUE_GRAPHICS_BIT;
        const bool has_compute = families[idx].queueFlags & VK_QUEUE_COMPUTE_BIT;
        const bool has_transfer = families[idx].queueFlags & VK_QUEUE_TRANSFER_BIT;

        if (has_graphics && graphics == INVALID)
            graphics = idx;

        if (has_compute && any_compute == INVALID)
            any_compute = idx;

        if (has_compute && !has_graphics && dedicated_compute == INVALID)
            dedicated_compute = idx;

        if (has_transfer && !has_graphics && !has_compute && dedicated_transfer == INVALID)
            dedicated_transfer = idx;
    }

    if (graphics == INVALID || any_compute == INVALID)
        return false;

    const bool graphics_has_compute = families[graphics].queueFlags & VK_QUEUE_COMPUTE_BIT;

    selection.graphics = graphics;
    selection.compute = graphics_has_compute ? graphics : any_compute;
    selection.transfer = graphics;

    if (dedicated_compute != INVALID)
        selection.compute = dedicated_compute;

    if (dedicated_transfer != INVALID)
        selection.transfer = dedicated_transfer;

    return true;
}

void VulkanDevice::select_physical_device(const DeviceRequirements& requirements)
{
    uint32_t physical_devices_count;
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &physical_devices_count, nullptr));

    std::vector<VkPhysicalDevice> physical_devices{physical_devices_count};
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &physical_devices_count, physical_devices.data()));

    constexpr int32_t DISCRETE_GPU_SCORE = 40;
    constexpr int32_t DEDICATED_QUEUE_SCORE = 10;

    int32_t best_score = -1;

    for (const VkPhysicalDevice& device : physical_devices)
    {
        VulkanPhysicalDeviceInfo info{};
        query_physical_device_info(device, info);

        const char* missing_requirement = find_missing_requirement(info, requirements);
        if (missing_requirement != nullptr)
        {
            MIZU_LOG_WARNING(
                "Skipping physical device '{}', missing requirement: {}",
                info.properties.deviceName,
                missing_requirement);
            continue;
        }

        QueueFamilies families{};
        if (!select_queue_families(info.queue_families, families))
        {
            MIZU_LOG_WARNING(
                "Skipping physical device '{}', missing graphics or compute queue", info.properties.deviceName);
            continue;
        }

        int32_t score = 0;

        if (info.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            score += DISCRETE_GPU_SCORE;

        if (families.compute != families.graphics)
            score += DEDICATED_QUEUE_SCORE;

        if (families.transfer != families.graphics)
            score += DEDICATED_QUEUE_SCORE;

        if (score <= best_score)
            continue;

        best_score = score;

        m_physical_device = device;
        m_queue_families = families;

        // TODO: Sparse binds are submitted on the graphics queue, pick a sparse capable queue instead
        const bool sparse_queue = info.queue_families[families.graphics].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT;

        m_optional_features = OptionalFeatures{};

        if (info.has_ray_tracing_extensions)
        {
            m_optional_features.ray_tracing = are_features_supported(info.features, RAY_TRACING_FEATURES);
        }

        if (sparse_queue)
        {
            m_optional_features.sparse_buffers = are_features_supported(info.features, SPARSE_BUFFER_FEATURES);
            m_optional_features.sparse_images = are_features_supported(info.features, SPARSE_IMAGE_FEATURES);
        }
    }

    MIZU_VERIFY(m_physical_device != VK_NULL_HANDLE, "Failed to find a suitable Vulkan physical device");
}

void VulkanDevice::create_device(const DeviceRequirements& requirements)
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

    inplace_vector<const char*, 8> device_extensions;

    if (requirements.swapchain)
    {
        for (const char* extension : SWAPCHAIN_EXTENSIONS)
            device_extensions.push_back(extension);
    }

    if (m_optional_features.ray_tracing)
    {
        for (const char* extension : RAY_TRACING_EXTENSIONS)
            device_extensions.push_back(extension);
    }

    VulkanFeatureChain supported{};
    add_feature_structures(supported, OPTIONAL_FEATURES);
    vkGetPhysicalDeviceFeatures2(m_physical_device, &supported.head());

    VulkanFeatureChain enabled{};
    enable_features(enabled, REQUIRED_FEATURES);

    for (const VulkanFeature& feature : OPTIONAL_FEATURES)
    {
        feature.get(enabled) = feature.get(supported);
    }

    if (m_optional_features.ray_tracing)
    {
        enable_features(enabled, RAY_TRACING_FEATURES);
    }

    if (m_optional_features.sparse_buffers)
    {
        enable_features(enabled, SPARSE_BUFFER_FEATURES);
    }

    if (m_optional_features.sparse_images)
    {
        enable_features(enabled, SPARSE_IMAGE_FEATURES);
    }

    // Create device
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &enabled.head();
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();
    create_info.pEnabledFeatures = nullptr;

    VK_CHECK(vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device));

    const VkPhysicalDeviceProperties properties = get_physical_device_properties(m_physical_device);

    m_properties = DeviceProperties{
        .name = properties.deviceName,
        .depth_clamp = enabled.head().features.depthClamp == VK_TRUE,
        .async_compute = m_queue_families.compute != m_queue_families.graphics,
        .async_transfer = m_queue_families.transfer != m_queue_families.graphics,
        .max_bindless_descriptors = NUM_BINDLESS_DESCRIPTORS,
        .alignments =
            DeviceAlignments{
                .min_constant_buffer_offset = properties.limits.minUniformBufferOffsetAlignment,
                .min_raw_buffer_offset = properties.limits.minStorageBufferOffsetAlignment,
                .min_texture_row_pitch = 1,
                .min_texture_data_placement = properties.limits.optimalBufferCopyOffsetAlignment,
            },
        .ray_tracing =
            RayTracingProperties{
                .supported = m_optional_features.ray_tracing,
                .acceleration_structure_instance_size = sizeof(VkAccelerationStructureInstanceKHR),
            },
        .sparse =
            SparseResourcesProperties{
                .buffers = m_optional_features.sparse_buffers,
                .images = m_optional_features.sparse_images,
                .non_resident_strict = properties.sparseProperties.residencyNonResidentStrict == VK_TRUE,
                .address_space_size = properties.limits.sparseAddressSpaceSize,
            },
    };

    // Retrieve queues
    std::unordered_map<uint32_t, VulkanQueue*> queue_family_to_queue;
    auto get_queue_or_insert = [&](uint32_t queue_family_idx, CommandBufferType type) -> VulkanQueue* {
        const auto it = queue_family_to_queue.find(queue_family_idx);
        if (it == queue_family_to_queue.end())
        {
            VkQueue queue;
            vkGetDeviceQueue(m_device, queue_family_idx, 0, &queue);

            VulkanQueue* q = m_queues.emplace_back(std::make_unique<VulkanQueue>(queue, queue_family_idx, type)).get();
            queue_family_to_queue.insert({queue_family_idx, q});
            return q;
        }

        return it->second;
    };

    m_graphics_queue = get_queue_or_insert(m_queue_families.graphics, CommandBufferType::Graphics);
    m_compute_queue = get_queue_or_insert(m_queue_families.compute, CommandBufferType::Compute);
    m_transfer_queue = get_queue_or_insert(m_queue_families.transfer, CommandBufferType::Transfer);

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
// Operations
//

void VulkanDevice::prepare_frame(uint32_t frame_in_flight_idx)
{
    VulkanContext.descriptor_manager->reset_transient(frame_in_flight_idx);
}

void VulkanDevice::wait_idle() const
{
    // Goes through the queues instead of vkDeviceWaitIdle so each queue's external synchronization is respected
    for (const std::unique_ptr<VulkanQueue>& queue : m_queues)
    {
        queue->wait_idle();
    }
}

//
// Creation functions
//

std::shared_ptr<BufferResource> VulkanDevice::create_buffer(
    const BufferDescription& desc,
    const MemoryPlacement& placement) const
{
    return std::make_shared<VulkanBufferResource>(desc, placement);
}

std::shared_ptr<ImageResource> VulkanDevice::create_image(
    const ImageDescription& desc,
    const MemoryPlacement& placement) const
{
    return std::make_shared<VulkanImageResource>(desc, placement);
}

std::shared_ptr<BufferResource> VulkanDevice::create_sparse_buffer(const BufferDescription& desc) const
{
    return std::make_shared<VulkanBufferResource>(desc, VulkanBufferResource::Sparse);
}

std::shared_ptr<ImageResource> VulkanDevice::create_sparse_image(const ImageDescription& desc) const
{
    return std::make_shared<VulkanImageResource>(desc, VulkanImageResource::Sparse);
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

DescriptorSetLayoutHandle VulkanDevice::create_descriptor_set_layout(const DescriptorSetLayoutDescription& desc) const
{
    return VulkanContext.descriptor_set_layout_cache->create(desc);
}

PipelineLayoutHandle VulkanDevice::create_pipeline_layout(const PipelineLayoutDescription& desc) const
{
    return VulkanContext.pipeline_layout_cache->create(desc);
}

std::shared_ptr<DescriptorSet> VulkanDevice::allocate_descriptor_set(
    DescriptorSetLayoutHandle layout,
    DescriptorSetAllocationType type,
    uint32_t variable_count) const
{
    switch (type)
    {
    case DescriptorSetAllocationType::Transient:
        return VulkanContext.descriptor_manager->allocate_transient(layout);
    case DescriptorSetAllocationType::Persistent:
        return VulkanContext.descriptor_manager->allocate_persistent(layout);
    case DescriptorSetAllocationType::Bindless:
        return VulkanContext.descriptor_manager->allocate_bindless(layout, variable_count);
    }
}

std::shared_ptr<Fence> VulkanDevice::create_fence(uint64_t initial_value) const
{
    return std::make_shared<VulkanFence>(initial_value);
}

std::shared_ptr<Swapchain> VulkanDevice::create_swapchain(const SwapchainDescription& desc) const
{
    return std::make_shared<VulkanSwapchain>(desc);
}

std::shared_ptr<GpuHeap> VulkanDevice::create_gpu_heap(const GpuHeapDescription& desc) const
{
    return std::make_shared<VulkanGpuHeap>(desc);
}

//
// Memory
//

MemoryRequirements VulkanDevice::get_buffer_memory_requirements(const BufferDescription& desc) const
{
    return get_vulkan_buffer_memory_requirements(desc);
}

MemoryRequirements VulkanDevice::get_image_memory_requirements(const ImageDescription& desc) const
{
    return get_vulkan_image_memory_requirements(desc);
}

std::optional<MemoryTypeId> VulkanDevice::find_memory_type(const MemoryRequirements& reqs, HeapDomain domain) const
{
    return find_memory_type_index(domain, reqs.memory_type_bits);
}

} // namespace Mizu::Vulkan

extern "C" MIZU_RENDER_CORE_VULKAN_API Mizu::Device* create_rhi_device(const Mizu::DeviceCreationDescription& desc)
{
    return new Mizu::Vulkan::VulkanDevice{desc};
}
