#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stack>
#include <thread>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

#include "base/debug/assert.h"

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/renderer.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanInstance;
class VulkanQueue;

class VulkanDevice
{
  public:
    VulkanDevice(const VulkanInstance& instance, const std::vector<const char*>& instance_extensions);
    ~VulkanDevice();

    [[nodiscard]] std::shared_ptr<VulkanQueue> get_graphics_queue() const;
    [[nodiscard]] std::shared_ptr<VulkanQueue> get_compute_queue() const;
    [[nodiscard]] std::shared_ptr<VulkanQueue> get_transfer_queue() const;

    VkCommandBuffer allocate_command_buffer(CommandBufferType type);
    std::vector<VkCommandBuffer> allocate_command_buffers(uint32_t count, CommandBufferType type);

    void free_command_buffer(VkCommandBuffer command_buffer, CommandBufferType type);
    void free_command_buffers(std::span<VkCommandBuffer> command_buffers, CommandBufferType type);

    [[nodiscard]] std::optional<uint32_t> find_memory_type(uint32_t filter, VkMemoryPropertyFlags properties) const;

    bool has_extension(const char* extension) const;

    RendererCapabilities get_physical_device_capabilities() const { return m_capabilities; }

    [[nodiscard]] VkDevice handle() const { return m_device; }
    [[nodiscard]] VkPhysicalDevice physical_device() const { return m_physical_device; }

  private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkPhysicalDevice m_physical_device{VK_NULL_HANDLE};

    RendererCapabilities m_capabilities{};

    struct QueueFamilies
    {
        uint32_t graphics;
        uint32_t compute;
        uint32_t transfer;
    };
    QueueFamilies m_queue_families{};

    std::shared_ptr<VulkanQueue> m_graphics_queue = nullptr;
    std::shared_ptr<VulkanQueue> m_compute_queue = nullptr;
    std::shared_ptr<VulkanQueue> m_transfer_queue = nullptr;

    struct ThreadCommandInfo
    {
        struct Type
        {
            VkCommandPool command_pool = VK_NULL_HANDLE;
            std::stack<VkCommandBuffer> available_command_buffers{};
            uint32_t command_buffers_in_usage = 0;
        };

        Type graphics;
        Type compute;
        Type transfer;

        Type& get_type(CommandBufferType type)
        {
            switch (type)
            {
            case CommandBufferType::Graphics:
                return graphics;
            case CommandBufferType::Compute:
                return compute;
            case CommandBufferType::Transfer:
                return transfer;
            }

            MIZU_UNREACHABLE("Invalid CommandBufferType");
        }
    };

    std::vector<ThreadCommandInfo> m_per_thread_command_info;

    std::stack<uint32_t> m_available_per_thread_command_info_idx;
    std::unordered_map<std::thread::id, uint32_t> m_thread_to_command_info_map;
    std::mutex m_assign_thread_info_mutex;

    void select_physical_device(const VulkanInstance& instance);
    void retrieve_physical_device_capabilities();

    static std::vector<VkExtensionProperties> get_physical_device_extension_properties(
        VkPhysicalDevice physical_device);
    static VkPhysicalDeviceProperties get_physical_device_properties(VkPhysicalDevice physical_device);
    static VkPhysicalDeviceFeatures get_physical_device_features(VkPhysicalDevice physical_device);
    static std::vector<VkQueueFamilyProperties> get_queue_family_properties(VkPhysicalDevice physical_device);

    ThreadCommandInfo create_thread_command_info();
    ThreadCommandInfo::Type& get_thread_command_info(std::thread::id id, CommandBufferType type);
};

} // namespace Mizu::Vulkan